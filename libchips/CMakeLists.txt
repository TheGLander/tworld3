cmake_minimum_required(VERSION 3.5)

project(libchips LANGUAGES C)
# set(CMAKE_C_FLAGS -fsanitize=address)

set(libchips_src)
list(APPEND libchips_src
  src/logic-generic.c
  src/logic-lynx.c
  src/logic-ms.c
  src/random.c
  src/misc.c
  src/format-ccl.c
)

# add_executable(libchips-cli ${libchips_src} src/main-cli.c)

add_library(chips SHARED ${libchips_src})

