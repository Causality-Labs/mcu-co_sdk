# A deliverable, unlike examples/, so it keeps the library's warning set.
set(CLI_COMPILE_OPTIONS
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

# The parser is its own target so a test can link it without pulling in main().
add_library(cli_common OBJECT
  mcuco_args.c
  mcuco_command.c
)

target_include_directories(cli_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cli_common PUBLIC mcuco)
target_compile_options(cli_common PRIVATE ${CLI_COMPILE_OPTIONS})

add_executable(mcu-co-cli
  main.c
)

target_link_libraries(mcu-co-cli PRIVATE cli_common)
target_compile_options(mcu-co-cli PRIVATE ${CLI_COMPILE_OPTIONS})

# Every CLI test lives here rather than in library/unit_tests: the spy defines
# the mcuco_* symbols itself, so it cannot be linked beside the real libmcuco
# that unit_tests needs.
if(NOT CMAKE_CROSSCOMPILING)
  # The root project declares LANGUAGES C; CppUTest's harness is C++, and unlike
  # library/ this directory has no project() of its own to enable it.
  enable_language(CXX)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  add_executable(cli_tests
    tests/AllTests.cpp
    tests/mcuco_spy.c
    tests/test_mcuco_spy.cpp
    tests/test_mcuco_args.cpp
    tests/test_mcuco_command.cpp
    mcuco_args.c
    mcuco_command.c
  )

  target_include_directories(cli_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/library/include
  )

  target_link_libraries(cli_tests PRIVATE CppUTest)

  add_test(NAME cli_tests COMMAND cli_tests -v)
endif()
