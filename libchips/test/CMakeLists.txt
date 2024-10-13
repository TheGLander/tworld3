# 'libchips_tests' is the subproject name
project(libchips_tests)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(../src) #allow referencing source as if we were in the src dir
include_directories(.)

file(GLOB_RECURSE GLOB_SRCS ../src/*.h ../src/*.c ../src/*.cpp)
message("sources")
message(${GLOB_SRCS})

file(GLOB_RECURSE GLOB_TEST_SRCS *.h *.c *.cpp)

# 'libchipsTests' is the target name
add_executable(lib_chips_tests ${GLOB_TEST_SRCS} ${GLOB_SRCS})
target_link_libraries(lib_chips_tests gtest gmock gtest_main)
