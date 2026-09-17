add_library(example_support STATIC status_name.c)
target_link_libraries(example_support PUBLIC mcuco)
target_include_directories(example_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(blink blink.c)
target_link_libraries(blink PRIVATE example_support)

add_executable(pwm_frequency pwm_frequency.c)
target_link_libraries(pwm_frequency PRIVATE example_support)

add_executable(irq_bind irq_bind.c)
target_link_libraries(irq_bind PRIVATE example_support)
