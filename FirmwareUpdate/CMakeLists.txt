cmake_minimum_required(VERSION 3.22)

# Firmware image for the STM32F767 (Cortex-M7, HAL + FreeRTOS + LwIP).
# Mirrors the CubeMX-generated Makefile; keep the two in sync when CubeMX
# regenerates the project.
#
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake
#   cmake --build build

project(FirmwareUpdate C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Emit compile_commands.json for the VS Code C/C++ extension and clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F767XX_FLASH.ld)

#######################################
# sources
#######################################
set(C_SOURCES
    Core/Src/main.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/usb_otg.c
    Core/Src/stm32f7xx_it.c
    Core/Src/stm32f7xx_hal_msp.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_cortex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_rcc_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_flash_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_gpio.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_dma_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pwr_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_i2c_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_exti.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_eth.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_uart_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_pcd_ex.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_ll_usb.c
    Core/Src/system_stm32f7xx.c
    Core/Src/sysmem.c
    Core/Src/syscalls.c
    Core/Src/freertos.c
    Core/Src/stm32f7xx_hal_timebase_tim.c
    LWIP/App/lwip.c
    LWIP/Target/ethernetif.c
    Drivers/BSP/Components/lan8742/lan8742.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim.c
    Drivers/STM32F7xx_HAL_Driver/Src/stm32f7xx_hal_tim_ex.c
    Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    Middlewares/Third_Party/FreeRTOS/Source/list.c
    Middlewares/Third_Party/FreeRTOS/Source/queue.c
    Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    Middlewares/Third_Party/FreeRTOS/Source/timers.c
    Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1/port.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/auth.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/ccp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/chap_ms.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/chap-md5.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/chap-new.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/demand.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/eap.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/eui64.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/fsm.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/ipcp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/ipv6cp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/lcp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/magic.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/mppe.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/multilink.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/ppp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/pppapi.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/pppcrypt.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/pppoe.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/pppol2tp.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/pppos.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/upap.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/utils.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/vj.c
    Middlewares/Third_Party/LwIP/src/netif/bridgeif.c
    Middlewares/Third_Party/LwIP/src/netif/bridgeif_fdb.c
    Middlewares/Third_Party/LwIP/src/netif/ethernet.c
    Middlewares/Third_Party/LwIP/src/netif/lowpan6.c
    Middlewares/Third_Party/LwIP/src/netif/lowpan6_ble.c
    Middlewares/Third_Party/LwIP/src/netif/lowpan6_common.c
    Middlewares/Third_Party/LwIP/src/netif/slipif.c
    Middlewares/Third_Party/LwIP/src/netif/zepif.c
    Middlewares/Third_Party/LwIP/src/netif/ppp/ecp.c
    Middlewares/Third_Party/LwIP/src/api/sockets.c
    Middlewares/Third_Party/LwIP/src/api/err.c
    Middlewares/Third_Party/LwIP/src/api/api_msg.c
    Middlewares/Third_Party/LwIP/src/api/netbuf.c
    Middlewares/Third_Party/LwIP/src/api/netifapi.c
    Middlewares/Third_Party/LwIP/src/api/tcpip.c
    Middlewares/Third_Party/LwIP/src/api/api_lib.c
    Middlewares/Third_Party/LwIP/src/api/netdb.c
    Middlewares/Third_Party/LwIP/src/api/if_api.c
    Middlewares/Third_Party/LwIP/src/core/tcp.c
    Middlewares/Third_Party/LwIP/src/core/sys.c
    Middlewares/Third_Party/LwIP/src/core/stats.c
    Middlewares/Third_Party/LwIP/src/core/memp.c
    Middlewares/Third_Party/LwIP/src/core/udp.c
    Middlewares/Third_Party/LwIP/src/core/raw.c
    Middlewares/Third_Party/LwIP/src/core/altcp.c
    Middlewares/Third_Party/LwIP/src/core/mem.c
    Middlewares/Third_Party/LwIP/src/core/altcp_alloc.c
    Middlewares/Third_Party/LwIP/src/core/def.c
    Middlewares/Third_Party/LwIP/src/core/inet_chksum.c
    Middlewares/Third_Party/LwIP/src/core/ip.c
    Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c
    Middlewares/Third_Party/LwIP/src/core/timeouts.c
    Middlewares/Third_Party/LwIP/src/core/tcp_in.c
    Middlewares/Third_Party/LwIP/src/core/init.c
    Middlewares/Third_Party/LwIP/src/core/tcp_out.c
    Middlewares/Third_Party/LwIP/src/core/pbuf.c
    Middlewares/Third_Party/LwIP/src/core/dns.c
    Middlewares/Third_Party/LwIP/src/core/netif.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/dhcp.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/igmp.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/autoip.c
    Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/ip6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/nd6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_addr.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/dhcp6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_frag.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/inet6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/ethip6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/icmp6.c
    Middlewares/Third_Party/LwIP/src/core/ipv6/mld6.c
    Middlewares/Third_Party/LwIP/system/OS/sys_arch.c
    Middlewares/Third_Party/LwIP/src/apps/mqtt/mqtt.c
)

set(ASM_SOURCES
    startup_stm32f767xx.s
)

add_executable(${PROJECT_NAME}.elf ${C_SOURCES} ${ASM_SOURCES})

#######################################
# includes and defines
#######################################
target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
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

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_HAL_DRIVER
    STM32F767xx
)

#######################################
# compile options
#######################################
# Cortex-M7 with the double-precision FPU, hard float ABI.
set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    ${MCU_FLAGS}
    -Wall
    # Place each function/datum in its own section so --gc-sections can drop
    # everything the firmware does not reference.
    -fdata-sections
    -ffunction-sections
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-gdwarf-2>
    $<$<CONFIG:Release>:-Os>
)

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
