# Demo code, not the deliverable: analysis runs on the library. Left on, every
# ignored printf return trips cert-err33-c and buries real findings.
set(CMAKE_C_CLANG_TIDY "")
set(CMAKE_C_CPPCHECK "")

add_executable(blink blink.c)
target_link_libraries(blink PRIVATE mcuco)

add_executable(pwm_frequency pwm_frequency.c)
target_link_libraries(pwm_frequency PRIVATE mcuco)

add_executable(irq_bind irq_bind.c)
target_link_libraries(irq_bind PRIVATE mcuco)
