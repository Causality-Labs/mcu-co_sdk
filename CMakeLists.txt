cmake_minimum_required(VERSION 3.22)

project(mcu-co VERSION 0.1.0
        DESCRIPTION "Host side of the mcu-co MCU co-processor"
        LANGUAGES C)

enable_testing()

add_subdirectory(library)
add_subdirectory(examples)
