cmake_minimum_required(VERSION 3.22)

# Firmware image for the STM32F767 (Cortex-M7, HAL + FreeRTOS + LwIP).
# Mirrors the CubeMX-generated Makefile; keep the two in sync when CubeMX
# regenerates the project.
#
#   cmake --preset Debug
#   cmake --build --preset Debug
#
# or explicitly:
#
#   cmake -B build/Debug -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
#   cmake --build build/Debug

# --- must be set before project() -------------------------------------------
# We target bare metal, so CMake's default compiler probe -- which links a full
# hosted executable -- cannot succeed: newlib's exit/_close/_sbrk have no
# implementation until our own syscalls.c and the linker script are in play.
# Building a static library for the probe instead is the supported workaround.
#
# The toolchain file sets this too, but it is repeated here deliberately: the
# VS Code CMake Tools extension configures with a bare
# -DCMAKE_C_COMPILER=... and no -DCMAKE_TOOLCHAIN_FILE, so without this the
# probe fails before project() ever returns.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT CMAKE_SYSTEM_NAME)
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR arm)
endif()

# Fall back to the toolchain from PATH when no compiler was supplied at all.
if(NOT CMAKE_C_COMPILER AND NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
    set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
endif()
# ----------------------------------------------------------------------------

project(FirmwareUpdate C CXX ASM)

# Run the preprocessor over .s files, matching the Makefile's
# "AS = arm-none-eabi-gcc -x assembler-with-cpp". Set here as well as in the
# toolchain file so it survives a configure that bypasses the toolchain file.
string(APPEND CMAKE_ASM_FLAGS " -x assembler-with-cpp")

if(NOT CMAKE_OBJCOPY)
    find_program(CMAKE_OBJCOPY arm-none-eabi-objcopy REQUIRED)
endif()
if(NOT CMAKE_SIZE)
    find_program(CMAKE_SIZE arm-none-eabi-size REQUIRED)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emit compile_commands.json for the VS Code C/C++ extension and clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F767XX_FLASH.ld)

#######################################
# sources
#######################################
# Globbed rather than listed. CONFIGURE_DEPENDS makes CMake re-run the glob at
# build time, so adding or deleting a .c file is picked up without a manual
# reconfigure.
#
# Drivers/CMSIS is deliberately NOT globbed: it ships the CMSIS DSP and NN
# libraries plus their test suites (490 .c files) which this firmware does not
# build. CMSIS contributes headers only -- the one CMSIS-related source we do
# compile, system_stm32f7xx.c, lives in Core/Src.
file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Core/Src/*.c
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/*.c
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F7xx_HAL_Driver/Src/*.c
    ${CMAKE_SOURCE_DIR}/ILT_OS/Lib/*.c
    ${CMAKE_SOURCE_DIR}/LWIP/*.c
    ${CMAKE_SOURCE_DIR}/Middlewares/*.c
)

file(GLOB_RECURSE CXX_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/Core/Src/*.cpp
    ${CMAKE_SOURCE_DIR}/ILT_OS/Lib/*.cpp
)

file(GLOB ASM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/*.s
)

list(LENGTH C_SOURCES _n_c)
list(LENGTH CXX_SOURCES _n_cxx)
message(STATUS "FirmwareUpdate: ${_n_c} C sources, ${_n_cxx} C++ sources, linker script ${LINKER_SCRIPT}")

# Cortex-M7 with the double-precision FPU, hard float ABI.
set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

add_executable(${PROJECT_NAME}.elf ${C_SOURCES} ${CXX_SOURCES} ${ASM_SOURCES})

#######################################
# includes and defines
#######################################
#######################################
# platform: what every translation unit in this firmware needs
#######################################
# An INTERFACE target so the selected application compiles with exactly the same
# include paths, defines and MCU flags as the firmware itself. Without this an
# application subdirectory would build for the host ABI and fail to link.
add_library(ilt_platform INTERFACE)

target_include_directories(ilt_platform INTERFACE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/ILT_OS/Lib
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F7xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32F7xx_HAL_Driver/Inc/Legacy
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32F7xx/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/LWIP/App
    ${CMAKE_SOURCE_DIR}/LWIP/Target
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/system
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1
    ${CMAKE_SOURCE_DIR}/Drivers/BSP/Components/lan8742
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/netif/ppp
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/lwip
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/lwip/apps
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/lwip/priv
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/lwip/prot
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/netif
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/compat/posix
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/compat/posix/net
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/compat/posix/sys
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/src/include/compat/stdc
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/LwIP/system/arch
)

target_compile_definitions(ilt_platform INTERFACE
    USE_HAL_DRIVER
    STM32F767xx
)

#######################################
# compile options
#######################################
target_compile_options(ilt_platform INTERFACE
    ${MCU_FLAGS}
    -Wall
    # Place each function/datum in its own section so --gc-sections can drop
    # everything the firmware does not reference.
    -fdata-sections
    -ffunction-sections
    # Bare metal: no unwinder, no type-info tables, and no __cxa_guard_*
    # locking around function-local statics.
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-gdwarf-2>
    $<$<CONFIG:Release>:-Os>
)

#######################################
# application selection
#######################################
# Exactly one application from ILT_OS/Applications is built into the image.
# Its own CMakeLists.txt declares the flags that shape it, e.g.
#   cmake --preset Debug -DILT_APP=Blinky -DBLINKY_LED=LD3
set(ILT_APP "Blinky" CACHE STRING
    "Application under ILT_OS/Applications to build into the image")

file(GLOB _ilt_app_dirs RELATIVE ${CMAKE_SOURCE_DIR}/ILT_OS/Applications
     ${CMAKE_SOURCE_DIR}/ILT_OS/Applications/*)
set(_ilt_apps "")
foreach(_d ${_ilt_app_dirs})
    if(EXISTS ${CMAKE_SOURCE_DIR}/ILT_OS/Applications/${_d}/CMakeLists.txt)
        list(APPEND _ilt_apps ${_d})
    endif()
endforeach()
set_property(CACHE ILT_APP PROPERTY STRINGS ${_ilt_apps})

if(NOT ILT_APP IN_LIST _ilt_apps)
    message(FATAL_ERROR
            "ILT_APP='${ILT_APP}' not found. Available: ${_ilt_apps}")
endif()

message(STATUS "Application: ${ILT_APP}")
add_subdirectory(ILT_OS/Applications/${ILT_APP})

# ilt_app is an OBJECT library, so this splices its objects straight into the
# executable -- no archive member selection to reason about.
target_link_libraries(${PROJECT_NAME}.elf PRIVATE ilt_platform ilt_app)

#######################################
# link options
#######################################
target_link_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -Wl,-Map=${PROJECT_NAME}.map,--cref
    -Wl,--gc-sections
)

# Linked as libraries rather than as link options so CMake places them *after*
# the object files. Order matters: newlib's libnosys.a supplies weak stubs for
# _sbrk/_exit/_close/_lseek, and Core/Src/syscalls.c + sysmem.c provide the real
# ones. If the libraries came first they would win and the link would fail with
# "multiple definition of _sbrk".
target_link_libraries(${PROJECT_NAME}.elf PRIVATE c m nosys)

# Relink when the linker script changes.
set_target_properties(${PROJECT_NAME}.elf PROPERTIES LINK_DEPENDS ${LINKER_SCRIPT})

#######################################
# post-build: size report, .hex and .bin
#######################################
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${PROJECT_NAME}.elf> ${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)
