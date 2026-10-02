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
  commands.c
)

target_include_directories(cli_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cli_common PUBLIC mcuco)
target_compile_options(cli_common PRIVATE ${CLI_COMPILE_OPTIONS})

add_executable(mcu-co-cli
  main.c
)

target_link_libraries(mcu-co-cli PRIVATE cli_common)
target_compile_options(mcu-co-cli PRIVATE ${CLI_COMPILE_OPTIONS})

install(TARGETS mcu-co-cli)

# Joins library/'s unit_tests rather than a binary of its own, so one run and
# one -t filter cover both.
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  target_sources(unit_tests PRIVATE tests/test_commands.cpp)
  target_link_libraries(unit_tests PRIVATE cli_common)
endif()
