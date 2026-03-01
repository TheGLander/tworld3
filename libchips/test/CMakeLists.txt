# 'libchips_tests' is the subproject name
project(libchips_tests)

SET(CMAKE_CXX_STANDARD 26)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
if (WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(googletest)

include_directories(.)

set(lib_chips_test_src)
list(APPEND lib_chips_test_src
    ccl/format-ccl.cpp
    tws/format-tws.cpp
    logic/cclp_tws_playthrough.cpp
)

# 'libchipsTests' is the target name
add_executable(lib_chips_tests ${lib_chips_test_src})
target_link_libraries(lib_chips_tests PRIVATE chips gtest gmock gtest_main)
