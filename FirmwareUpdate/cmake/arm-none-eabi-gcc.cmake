# Toolchain file for the GNU Arm Embedded toolchain (arm-none-eabi).
# Use with:  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake

set(CMAKE_SYSTEM_NAME       Generic)
set(CMAKE_SYSTEM_PROCESSOR  arm)

# We are building a bare-metal firmware image, so CMake's default "compile and
# link a test executable" probe cannot succeed (no _start, no syscalls yet).
# Building a static library instead is the supported way to satisfy the probe.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Allow an explicit toolchain location:  -DTOOLCHAIN_PREFIX=/path/to/bin/
# Otherwise arm-none-eabi-* is taken from PATH.
set(TOOLCHAIN_PREFIX "" CACHE STRING "Directory containing arm-none-eabi-* binaries (with trailing slash)")

set(CMAKE_C_COMPILER    ${TOOLCHAIN_PREFIX}arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER  ${TOOLCHAIN_PREFIX}arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  ${TOOLCHAIN_PREFIX}arm-none-eabi-g++)
set(CMAKE_OBJCOPY       ${TOOLCHAIN_PREFIX}arm-none-eabi-objcopy CACHE FILEPATH "objcopy")
set(CMAKE_SIZE          ${TOOLCHAIN_PREFIX}arm-none-eabi-size    CACHE FILEPATH "size")

# Assemble .s/.S through gcc so the preprocessor runs, matching the Makefile's
# "AS = arm-none-eabi-gcc -x assembler-with-cpp".
set(CMAKE_ASM_FLAGS_INIT "-x assembler-with-cpp")

# Only look for programs on the host; headers/libraries come from the toolchain.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
