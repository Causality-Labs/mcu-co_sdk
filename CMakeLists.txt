cmake_minimum_required(VERSION 3.22)

project(mcu-co VERSION 0.1.0
        DESCRIPTION "Host side of the mcu-co MCU co-processor"
        LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_CERT_CHECK "Run CERT-C compliance checks with clang-tidy" OFF)
option(ENABLE_CPPCHECK   "Run general static analysis with cppcheck"    OFF)

if(ENABLE_CERT_CHECK)
    find_program(CLANG_TIDY clang-tidy REQUIRED)
    set(CMAKE_C_CLANG_TIDY
        ${CLANG_TIDY}
        --extra-arg=-Wno-unused-command-line-argument
    )
endif()

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK cppcheck REQUIRED)
    set(CMAKE_C_CPPCHECK
        ${CPPCHECK}
        --std=c11
        --enable=all
        --error-exitcode=1
        "--suppressions-list=${CMAKE_SOURCE_DIR}/cppcheck.supp"
        "--suppress=*:${CMAKE_BINARY_DIR}/_deps/*"
        -D__GNUC__
    )
endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE FORMAT_SOURCES
        "${CMAKE_SOURCE_DIR}/library/src/*.c"
        "${CMAKE_SOURCE_DIR}/library/include/*.h"
        "${CMAKE_SOURCE_DIR}/examples/*.c"
        "${CMAKE_SOURCE_DIR}/examples/*.h"
        "${CMAKE_SOURCE_DIR}/cli/*.c"
        "${CMAKE_SOURCE_DIR}/cli/*.h"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_SOURCES}
        COMMENT "Formatting sources with clang-format"
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${FORMAT_SOURCES}
        COMMENT "Checking source formatting with clang-format"
    )
endif()

enable_testing()

add_subdirectory(library)
add_subdirectory(examples)
