cmake_minimum_required(VERSION 3.5)

project(libchips LANGUAGES C)

set(libchips_src)
list(APPEND libchips_src
  src/logic-lynx.c
  src/logic-generic.c
  src/random.c
)

# add_executable(libchips-cli ${libchips_src} src/main-cli.c)

add_library(chips SHARED ${libchips_src})

