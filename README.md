# Discord Bot in C
No wrappers, no libraries, just pure masochism and `s2n-tls`.

### "Why did you code this in the first place?"
I asked a friend about coding a bot in C++ with a framework. He told me to do it in C without any wrappers instead. And I took that personally.

### "Why not Rust?"
Mainly to ragebait Rust fans. Also, because I like managing my own memory and knowing exactly how a WebSocket frame is masked.

### "How long did it take?"
I lost track of time somewhere between the TLS handshake and the WebSocket length encoding logic.

### "Can I use this for my project?"
Sure, but you probably shouldn't. This is a study in pain, not a production-grade framework.

## How to Run

### 1. Prerequisites
You'll need `meson`, `ninja`, and `clang` installed on your system (tested on Fedora/Linux).

### 2. Setup
```bash
git clone https://github.com/NonsensicalOne/discord-bot-in-c
cd discord-bot-c

meson setup build
```

### 3. Compile

```bash
meson compile -C build
```

### 4. Run

```bash
export DISCORD_TOKEN="your_token_here"
./build/main
```

## Technical Details

- **TLS 1.3:** Handled by `s2n-tls`.
- **WebSocket:** Raw implementation including masking and 126-bit extended payload length support.
- **JSON:** String-based parsing (no heavy external parsers yet).