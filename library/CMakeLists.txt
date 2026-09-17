cmake_minimum_required(VERSION 3.22)

project(mcuco VERSION 0.1.0
        DESCRIPTION "Host library for the mcu-co MCU co-processor"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(mcuco SHARED
  src/uart.c
  src/crc16.c
  src/protocol.c
  src/command_transport.c
  src/mcuco.c
)

target_include_directories(mcuco PUBLIC include)

target_compile_options(mcuco PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wsign-conversion
  -Wshadow
  -Wstrict-prototypes
  -Wmissing-prototypes
  -Werror
  -O0
  -g
)

set_target_properties(mcuco PROPERTIES
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Tests run on the host against a pty, so they are skipped when the build
# targets the board.
if(NOT CMAKE_CROSSCOMPILING)
  include(FetchContent)
  FetchContent_Declare(CppUTest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG        v4.0
    GIT_SHALLOW    TRUE
  )
  set(CPPUTEST_PLATFORM Gcc CACHE STRING "Platform implementation")
  set(TESTS OFF CACHE BOOL "Compile and make tests for CppUTest itself" FORCE)
  FetchContent_MakeAvailable(CppUTest)

  enable_testing()

  add_executable(unit_tests
    tests/AllTests.cpp
    tests/test_uart.cpp
    tests/test_crc16.cpp
    tests/test_protocol.cpp
    tests/test_command_transport.cpp
    tests/test_mcuco.cpp
  )
  target_link_libraries(unit_tests PRIVATE mcuco CppUTest util) # util: openpty

  add_test(NAME unit_tests COMMAND unit_tests -v)
endif()
