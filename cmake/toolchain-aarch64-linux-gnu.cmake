set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_PREFIX aarch64-linux-gnu-)

set(CMAKE_C_COMPILER    ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER  ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY       ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE          ${TOOLCHAIN_PREFIX}size)

# The Ubuntu cross packages are not a standalone sysroot: the compiler's own
# sysroot is / and it resolves libc through multiarch paths, so CMAKE_SYSROOT
# must stay unset here. Point find_* at the target tree instead.
# For the Yocto SDK, set CMAKE_SYSROOT to its sysroot and update this path.
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)

# Look for headers and libraries in the target tree, never the host's
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Cortex-A55 (Armv8.2-A) on the i.MX93. Tune, but stay binary-compatible
# with baseline Armv8-A so the output also runs under qemu and on other boards.
add_compile_options(-mtune=cortex-a55)
