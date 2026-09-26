# NUCLEO-F767ZI: STM32F767ZI (Cortex-M7, double-precision FPU), LAN8742A PHY on
# RMII, three user LEDs, ST-LINK VCP on USART3.
#
# Core/, Drivers/, LWIP/, the startup file, the linker script and the .ioc are
# the CubeMX-generated project for this board and live here so that regenerating
# from FirmwareUpdate.ioc touches only this directory. Board.cpp and
# BoardEthernet.cpp are ours: they implement BSP/Include/Bsp/*.h on top of it.

set(_board_dir ${CMAKE_CURRENT_SOURCE_DIR})

#######################################
# what the rest of the build compiles against
#######################################
# Cortex-M7 with the double-precision FPU, hard float ABI. Applied to compiles
# and to the link, so libgcc/libc multilib selection matches the objects.
set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(ilt_platform INTERFACE ${MCU_FLAGS})
target_link_options(ilt_platform INTERFACE ${MCU_FLAGS})

target_compile_definitions(ilt_platform INTERFACE
    USE_HAL_DRIVER
    STM32F767xx
    # This board's linker script provides a flash region reserved for the web
    # UI, so the generated asset is placed there rather than in .rodata.
    ILT_WEBSITE_SECTION
)

# Chip and board headers. FreeRTOSConfig.h (Core/Inc) and lwipopts.h
# (LWIP/Target) are board-specific tuning, which is why the middlewares get
# their configuration from here rather than the other way round.
target_include_directories(ilt_platform INTERFACE
    ${_board_dir}/Core/Inc
    ${_board_dir}/Drivers/STM32F7xx_HAL_Driver/Inc
    ${_board_dir}/Drivers/STM32F7xx_HAL_Driver/Inc/Legacy
    ${_board_dir}/Drivers/CMSIS/Device/ST/STM32F7xx/Include
    ${_board_dir}/Drivers/CMSIS/Include
    ${_board_dir}/Drivers/BSP/Components/lan8742
    ${_board_dir}/LWIP/App
    ${_board_dir}/LWIP/Target
)

#######################################
# board sources
#######################################
# Globbed rather than listed. CONFIGURE_DEPENDS makes CMake re-run the glob at
# build time, so a file added or removed by CubeMX is picked up without a manual
# reconfigure.
#
# Drivers/CMSIS is deliberately NOT globbed: it ships the CMSIS DSP and NN
# libraries plus their test suites (490 .c files) which this firmware does not
# build. CMSIS contributes headers only -- the one CMSIS-related source we do
# compile, system_stm32f7xx.c, lives in Core/Src.
file(GLOB_RECURSE _board_c_sources CONFIGURE_DEPENDS
    ${_board_dir}/Core/Src/*.c
    ${_board_dir}/Drivers/BSP/*.c
    ${_board_dir}/Drivers/STM32F7xx_HAL_Driver/Src/*.c
    ${_board_dir}/LWIP/Target/*.c
)

# LWIP/App/lwip.c is CubeMX's NO_SYS netif bring-up. We run lwIP in OS mode and
# bring the interface up from ilt::net::NetworkStack instead, so the file is
# left on disk for CubeMX to regenerate into but is not compiled -- building it
# would give us a second, unused netif and a duplicate sys_now().
list(FILTER _board_c_sources EXCLUDE REGEX "/LWIP/App/lwip\\.c$")

# BoardRig.cpp reports the rig absent (this Nucleo has none attached); the
# simulation stands in for it when asked for. Exactly one of the two is
# compiled -- they define the same symbols.
if(ILT_SIMULATE_RIG)
    set(_rig_source ${ILT_RIG_SIMULATION_SOURCE})
    message(STATUS "  Board: rig hardware SIMULATED (-DILT_SIMULATE_RIG=ON)")
else()
    set(_rig_source ${_board_dir}/BoardRig.cpp)
endif()

add_library(ilt_bsp OBJECT
    ${_board_c_sources}
    ${_board_dir}/Board.cpp
    ${_board_dir}/BoardEthernet.cpp
    ${_rig_source}
)

target_link_libraries(ilt_bsp PRIVATE ilt_platform)

# ethernetif.c and BoardEthernet.cpp include lwIP and FreeRTOS headers. Those
# include paths are added to ilt_platform by Middlewares/CMakeLists.txt, which
# is processed after this file; INTERFACE properties are resolved at generate
# time, so the ordering does not matter.

#######################################
# exported to the top level
#######################################
# Cache variables rather than PARENT_SCOPE: the top level is two directories up,
# and PARENT_SCOPE only reaches one.
set(ILT_STARTUP_SOURCE ${_board_dir}/startup_stm32f767xx.s
    CACHE INTERNAL "Startup assembly for the selected board")
set(ILT_LINKER_SCRIPT ${_board_dir}/STM32F767XX_FLASH.ld
    CACHE INTERNAL "Linker script for the selected board")

# Cortex-M7 r0p1 port. The H723 is also an M7 and uses the same port directory;
# a Cortex-M4 board would set this differently.
set(ILT_FREERTOS_PORT_DIR GCC/ARM_CM7/r0p1
    CACHE INTERNAL "FreeRTOS portable subdirectory for the selected board")

message(STATUS "  Board: NUCLEO-F767ZI (STM32F767ZI, LAN8742A RMII, USART3 VCP)")
