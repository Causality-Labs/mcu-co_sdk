# Unlike examples/, this is a deliverable, so it keeps the library's warning set
# and whatever analysis the parent build enabled.

add_executable(mcu-co-cli
  main.c
)

target_link_libraries(mcu-co-cli PRIVATE mcuco)

target_compile_options(mcu-co-cli PRIVATE
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
