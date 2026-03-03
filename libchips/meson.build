project(
  'libchips',
  'c', 'cpp',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=gnu23', 'cpp_std=gnu++20'],
)

sources = [
  'src/misc.c',
  'src/random.c',
  'src/logic-lynx.c',
  'src/format-ccl.c',
  'src/format-tws.c',
  'src/formats.c',
  'src/logic-generic.c',
  'src/logic-ms.c',
]

libchips_inc = include_directories('src')
libchips = library(
  'chips',
  sources,
  install: true
)

libchips_dep = declare_dependency(
  include_directories: libchips_inc,
  link_with: libchips
)

subdir('test')
