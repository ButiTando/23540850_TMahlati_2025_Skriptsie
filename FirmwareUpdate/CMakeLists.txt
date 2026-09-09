cmake_minimum_required(VERSION 3.22)

# Firmware image for the ILT degrader/dosimeter.
#
# The build is split so that one image can be produced for several boards:
#
#   BSP/Boards/<board>   everything chip- and board-specific: the CubeMX tree,
#                        the HAL, the startup file, the linker script, and the
#                        implementation of the BSP headers in BSP/Include.
#   Middlewares          FreeRTOS and lwIP -- chip-agnostic, built once.
#   ILT_OS/Lib           the portable C++ layer (threads, network, HTTP).
#   ILT_OS/Applications  exactly one application, selected with -DILT_APP.
#
#   cmake --preset Debug -DILT_BOARD=NucleoF767ZI -DILT_APP=WebServer
#   cmake --build --preset Debug
#
# Nothing above BSP/ may include a chip header directly; if it needs something
# from the board it goes through BSP/Include/Bsp/*.h, so adding the H723 is a
# matter of adding a board directory rather than editing application code.

# The board is chosen before project() because it decides whether this is a
# cross build at all. BSP/CMakeLists.txt declares the cache entry properly and
# validates it; here we only need to know if it is the host.
set(ILT_BOARD "NucleoF767ZI" CACHE STRING "Board under BSP/Boards to build for")
if(ILT_BOARD STREQUAL "Host")
    set(ILT_HOST_BUILD ON)
endif()

# --- must be set before project() -------------------------------------------
# Everything in this block is for the cross build only. The host build wants
# CMake's ordinary native toolchain detection, so it is skipped wholesale.
if(NOT ILT_HOST_BUILD)

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

endif() # NOT ILT_HOST_BUILD
# ----------------------------------------------------------------------------

if(ILT_HOST_BUILD)
    project(FirmwareUpdate C CXX)
else()
    project(FirmwareUpdate C CXX ASM)
endif()

if(NOT ILT_HOST_BUILD)
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

#######################################
# platform: what every translation unit in this firmware shares
#######################################
# An INTERFACE target carrying the compile flags, defines and include paths
# common to the firmware, the middlewares and the application, so all of them
# are compiled the same way. It is created empty here and filled in by the
# selected board, which is the only place that knows the MCU flags and where
# the chip headers live.
add_library(ilt_platform INTERFACE)

target_compile_options(ilt_platform INTERFACE
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
# subdirectories
#######################################
# BSP first: it populates ilt_platform with the MCU flags and chip include
# paths that Middlewares, ILT_OS and the application all compile against, and
# exports ILT_STARTUP_SOURCE and ILT_LINKER_SCRIPT for the link below.
add_subdirectory(BSP)
add_subdirectory(Middlewares)
add_subdirectory(ILT_OS)

#######################################
# image
#######################################
# The startup file is named here rather than left to ilt_bsp so the executable
# has a source of its own; every other object arrives from the OBJECT libraries
# linked below.
# On the host the executable is a plain binary with no startup file; on a board
# it is an .elf built around the board's startup assembly.
if(ILT_HOST_BUILD)
    set(ILT_IMAGE ${PROJECT_NAME})
    add_executable(${ILT_IMAGE})
else()
    set(ILT_IMAGE ${PROJECT_NAME}.elf)
    add_executable(${ILT_IMAGE} ${ILT_STARTUP_SOURCE})
endif()

# All four are OBJECT libraries, so this splices their objects straight into the
# executable -- no archive member selection to reason about, which matters for
# syscalls.c and for the application, neither of which is reached by a symbol
# the linker would otherwise go looking for.
target_link_libraries(${ILT_IMAGE} PRIVATE
    ilt_platform
    ilt_bsp
    ilt_middlewares
    ilt_os
    ilt_app
)

if(NOT ILT_HOST_BUILD)
    target_link_options(${ILT_IMAGE} PRIVATE
        -T${ILT_LINKER_SCRIPT}
        -specs=nano.specs
        -Wl,-Map=${PROJECT_NAME}.map,--cref
        -Wl,--gc-sections
        -Wl,--print-memory-usage
    )

    # Linked as libraries rather than as link options so CMake places them
    # *after* the object files. Order matters: newlib's libnosys.a supplies weak
    # stubs for _sbrk/_exit/_close/_lseek, and Core/Src/syscalls.c + sysmem.c
    # provide the real ones. If the libraries came first they would win and the
    # link would fail with "multiple definition of _sbrk".
    target_link_libraries(${ILT_IMAGE} PRIVATE c m nosys)

    # Relink when the linker script changes.
    set_target_properties(${ILT_IMAGE} PROPERTIES
                          LINK_DEPENDS ${ILT_LINKER_SCRIPT})
endif()

#######################################
# post-build: size report, .hex and .bin
#######################################
if(NOT ILT_HOST_BUILD)
add_custom_command(TARGET ${ILT_IMAGE} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${ILT_IMAGE}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${ILT_IMAGE}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${ILT_IMAGE}> ${PROJECT_NAME}.bin
    COMMENT "Generating ${PROJECT_NAME}.hex and ${PROJECT_NAME}.bin"
)
endif()

message(STATUS "")
message(STATUS "FirmwareUpdate configuration")
message(STATUS "  board       : ${ILT_BOARD}")
message(STATUS "  application : ${ILT_APP}")
message(STATUS "  build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "")
