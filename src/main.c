#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// s2n is gods gift to all programmers who are working with tls
#include <s2n.h>

// start helper functions

void read_websocket_buffer(int valread, char *buffer) {
  if (valread < 2)
    return;
  unsigned char *u_buf = (unsigned char *)buffer;

  if ((u_buf[0] & 0x0F) == 0x01) {
    int payload_len = u_buf[1] & 0x7F;
    char *data_ptr = (payload_len == 126) ? &buffer[4] : &buffer[2];
    int data_len =
        (payload_len == 126) ? ((u_buf[2] << 8) | u_buf[3]) : payload_len;

    printf("Discord says: %.*s\n", data_len, data_ptr);
  }
}

void send_websocket_text(struct s2n_connection *conn, const char *text) {
  size_t len = strlen(text);
  unsigned char frame[len + 10];
  int head = (len <= 125) ? 2 : 4;

  frame[0] = 0x81;
  if (len <= 125) {
    frame[1] = 0x80 | (len & 0x7F);
  } else {
    frame[1] = 0x80 | 126;
    frame[2] = (len >> 8) & 0xFF;
    frame[3] = len & 0xFF;
  }

  unsigned char mask[4] = {rand() % 256, rand() % 256, rand() % 256,
                           rand() % 256};
  memcpy(&frame[head], mask, 4);

  for (size_t i = 0; i < len; i++)
    frame[head + 4 + i] = text[i] ^ mask[i % 4];

  s2n_blocked_status blocked;
  s2n_send(conn, frame, head + 4 + len, &blocked);
}

void send_https_message(struct s2n_config *cfg, const char *chan,
                        const char *text, const char *token) {
  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM},
                  *res;

  if (getaddrinfo("discord.com", "443", &hints, &res) != 0) {
    fprintf(stderr, "REST: DNS lookup failed\n");
    return;
  }

  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd < 0) {
    fprintf(stderr, "REST: Socket creation failed\n");
    freeaddrinfo(res);
    return;
  }

  struct s2n_connection *conn = s2n_connection_new(S2N_CLIENT);
  s2n_connection_set_config(conn, cfg);
  s2n_connection_set_fd(conn, fd);
  s2n_set_server_name(conn, "discord.com");

  s2n_blocked_status blocked;
  s2n_negotiate(conn, &blocked);

  char body[512], req[1024];
  sprintf(body, "{\"content\":\"%s\"}", text);
  sprintf(req,
          "POST /api/v10/channels/%s/messages HTTP/1.1\r\n"
          "Host: discord.com\r\n"
          "Authorization: Bot %s\r\n"
          "Content-Type: application/json\r\n"
          "Content-Length: %zu\r\n"
          "Connection: close\r\n\r\n%s",
          chan, token, strlen(body), body);

  s2n_send(conn, req, strlen(req), &blocked);

  s2n_connection_free(conn);
  close(fd);
  freeaddrinfo(res);
}

// end helper functions

int main(void) {
  srand(time(NULL));
  s2n_init();

  // connection setup
  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM},
                  *res;
  if (getaddrinfo("gateway.discord.gg", "443", &hints, &res) != 0) {
    perror("getaddrinfo error");
    return -1;
  }

  int client_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (connect(client_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror("Socket creation error");
    return -1;
  }
  freeaddrinfo(res);

  // tls setup
  struct s2n_config *config = s2n_config_new();
  s2n_config_set_cipher_preferences(config, "default_tls13");
  struct s2n_connection *conn = s2n_connection_new(S2N_CLIENT);
  s2n_connection_set_config(conn, config);
  s2n_connection_set_fd(conn, client_fd);
  s2n_set_server_name(conn, "gateway.discord.gg");

  s2n_blocked_status blocked;
  s2n_negotiate(conn, &blocked);

  // handshake and authentication
  char buffer[16384];
  s2n_recv(conn, buffer, sizeof(buffer), &blocked); // HTTP 101
  s2n_recv(conn, buffer, sizeof(buffer), &blocked); // Hello

  send_websocket_text(
      conn, "{\"op\":1,\"d\":null}"); // first heartbeat that is needed

  char *token = getenv("DISCORD_TOKEN");
  char identify[1024];
  sprintf(
      identify,
      "{\"op\":2,\"d\":{\"token\":\"%s\",\"intents\":33280,"
      "\"properties\":{\"os\":\"linux\",\"browser\":\"c\",\"device\":\"c\"}}}",
      token);
  send_websocket_text(conn, identify);

  // yes, poll is slow, but we are not shipping to 1 million users
  struct pollfd fds[1] = {{.fd = client_fd, .events = POLLIN}};
  while (poll(fds, 1, 30000) >= 0) {
    if (fds[0].revents & POLLIN) {
      int valread = s2n_recv(conn, buffer, sizeof(buffer), &blocked);
      if (valread <= 0)
        break;

      read_websocket_buffer(valread, buffer);

      if (strstr(buffer, "\"content\":\"Hi\"")) {
        char *c_ptr = strstr(buffer, "\"channel_id\":\"");
        if (c_ptr) {
          char channel_id[32] = {0};
          char *start = c_ptr + 14;
          char *end = strchr(start, '\"');
          strncpy(channel_id, start, end - start);

          send_https_message(config, channel_id, "Hello from C!", token);
        }
      }
    } else {
      send_websocket_text(conn, "{\"op\":1,\"d\":null}");
    }
  }

  // always free your shit
  s2n_connection_free(conn);
  s2n_config_free(config);
  s2n_cleanup();
  close(client_fd);
  return 0;
}