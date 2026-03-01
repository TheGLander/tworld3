cmake_minimum_required(VERSION 3.5)

project(libchips LANGUAGES C)
# set(CMAKE_C_FLAGS -fsanitize=address)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(libchips_src)
list(APPEND libchips_src
  src/logic-generic.c
  src/logic-lynx.c
  src/logic-ms.c
  src/random.c
  src/misc.c
  src/format-ccl.c
  src/format-tws.c
  src/formats.c
)

# add_executable(libchips-cli ${libchips_src} src/main-cli.c)

add_library(chips SHARED ${libchips_src})

target_include_directories(chips PUBLIC src)

#for tests
add_subdirectory(test)
