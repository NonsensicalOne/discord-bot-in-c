project('discordbotinc', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11'])

add_project_arguments('-D_GNU_SOURCE', language: 'c')

s2n_sub = subproject('s2n-tls')

s2n_dep = s2n_sub.get_variable('s2n_tls_dep')

executable('main', 
           'src/main.c',
           dependencies: [s2n_dep],
           install : true)
