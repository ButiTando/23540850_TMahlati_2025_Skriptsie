# Host -- the firmware built as a native Linux process.
#
# Not a simulator: this is the same ILT_OS, the same lwIP and the same
# application, on the FreeRTOS POSIX port with a TAP device instead of an
# Ethernet MAC. What it exercises is everything above the driver line -- our
# threads and RTOS wrappers, HttpServer, StreamServer, the degrader and
# dosimeter logic. What it cannot exercise is the HAL, ethernetif.c, the linker
# layout, the MPU and the caches; those need real hardware or Renode.
#
#   cmake --preset Host && cmake --build --preset Host
#   sudo ip tuntap add dev tap0 mode tap user "$USER"
#   sudo ip addr add 192.168.7.1/24 dev tap0 && sudo ip link set tap0 up
#   PRECONFIGURED_TAPIF=tap0 ./build/Host/FirmwareUpdate
#   curl http://192.168.7.2/api/status.json

set(_board_dir ${CMAKE_CURRENT_SOURCE_DIR})

#######################################
# what the rest of the build compiles against
#######################################
target_compile_definitions(ilt_platform INTERFACE
    ILT_HOST_BUILD=1
    # The POSIX port hands every task stack to pthread_attr_setstack(), so no
    # stack may be smaller than glibc's PTHREAD_STACK_MIN. StaticThread rounds
    # up to this, which lets application thread sizes stay tuned for the MCU.
    ILT_MIN_THREAD_STACK_BYTES=262144
)

target_include_directories(ilt_platform INTERFACE
    ${_board_dir}                 # lwipopts.h
    ${_board_dir}/Core/Inc        # FreeRTOSConfig.h
    ${_board_dir}/compat          # cmsis_compiler.h stand-in
    ${_board_dir}/include         # netif/tapif.h
)

# The POSIX port is pthreads; everything links against it.
find_package(Threads REQUIRED)
target_link_libraries(ilt_platform INTERFACE Threads::Threads)

# lwIP's source list is shared with the boards and includes netifs this build
# never instantiates -- slipif wants sio_* from a serial port that does not
# exist here. The target drops them via --gc-sections; do the same rather than
# fork the source list. ilt_platform already compiles with -ffunction-sections.
target_link_options(ilt_platform INTERFACE -Wl,--gc-sections)

#######################################
# board sources
#######################################
if(ILT_SIMULATE_RIG)
    set(_rig_source ${ILT_RIG_SIMULATION_SOURCE})
    message(STATUS "  Board: rig hardware SIMULATED (-DILT_SIMULATE_RIG=ON)")
else()
    set(_rig_source ${_board_dir}/BoardRig.cpp)
endif()

add_library(ilt_bsp OBJECT
    ${_board_dir}/Board.cpp
    ${_board_dir}/BoardEthernet.cpp
    ${_rig_source}
    ${_board_dir}/main.cpp
    ${_board_dir}/netif/tapif.c
)

target_link_libraries(ilt_bsp PRIVATE ilt_platform)

# tapif.c is upstream lwIP contrib, built as shipped. LWIP_UNIX_LINUX selects
# the /dev/net/tun path and the Linux TUNSETIFF ioctl; without it the file falls
# through to the BSD branch and looks for /dev/tap0.
set_source_files_properties(${_board_dir}/netif/tapif.c PROPERTIES
    COMPILE_OPTIONS "-w"
    COMPILE_DEFINITIONS "LWIP_UNIX_LINUX")

#######################################
# exported to the top level
#######################################
# No startup assembly and no linker script: the C library provides the entry
# point and the kernel provides the memory map. ILT_STARTUP_SOURCE stays empty
# and the top level omits the MCU-only link options and post-build steps.
set(ILT_STARTUP_SOURCE "" CACHE INTERNAL "No startup file on the host")
set(ILT_LINKER_SCRIPT "" CACHE INTERNAL "No linker script on the host")

set(ILT_FREERTOS_PORT_DIR ThirdParty/GCC/Posix
    CACHE INTERNAL "FreeRTOS portable subdirectory for the selected board")

message(STATUS "  Board: Host (POSIX + TAP, FreeRTOS simulator port)")
