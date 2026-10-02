cmake_minimum_required(VERSION 3.22)

project(mcuco VERSION 0.1.0
        DESCRIPTION "Host library for the mcu-co MCU co-processor"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mcuco SHARED
  src/uart.c
  src/status.c
  src/crc16.c
  src/endianess.c
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

include(GNUInstallDirs)
install(TARGETS mcuco)
# The only header an application needs; the rest of include/ is internal.
install(FILES include/mcuco.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Tests run on the host against a pty, so they are skipped when the build
# cross-compiles.
if(NOT CMAKE_CROSSCOMPILING)
  include(FetchContent)
  set(CPPUTEST_PLATFORM Gcc CACHE STRING "Platform implementation")
  set(TESTS OFF CACHE BOOL "Compile and make tests for CppUTest itself" FORCE)

  # CppUTest brings its own install rules, and without EXCLUDE_FROM_ALL
  # `cmake --install` would put it on the system alongside mcuco. The keyword
  # on FetchContent_Declare needs CMake 3.28; older versions, such as the 3.25
  # in Raspberry Pi OS Bookworm, need the populate-then-add_subdirectory form,
  # which 3.30 and later deprecate.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.28)
    FetchContent_Declare(CppUTest
      GIT_REPOSITORY https://github.com/cpputest/cpputest.git
      GIT_TAG        v4.0
      GIT_SHALLOW    TRUE
      EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(CppUTest)
  else()
    FetchContent_Declare(CppUTest
      GIT_REPOSITORY https://github.com/cpputest/cpputest.git
      GIT_TAG        v4.0
      GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(CppUTest)
    if(NOT cpputest_POPULATED)
      FetchContent_Populate(CppUTest)
      add_subdirectory(${cpputest_SOURCE_DIR} ${cpputest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()
  endif()

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
