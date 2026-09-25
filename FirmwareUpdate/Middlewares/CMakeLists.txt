# FreeRTOS and lwIP.
#
# Both are chip-agnostic: they are configured by FreeRTOSConfig.h and
# lwipopts.h, which live in the selected board's directory and reach the
# compiler through ilt_platform. That is what lets a single copy of the
# middlewares serve every board.
#
# The trees are the CubeMX-selected subsets -- CubeMX has already pruned
# FreeRTOS/portable down to the one port and one heap implementation in use, so
# a recursive glob here cannot pick up a second port.c or a second heap_*.c.

set(_fr ${CMAKE_CURRENT_SOURCE_DIR}/Third_Party/FreeRTOS/Source)
set(_lwip ${CMAKE_CURRENT_SOURCE_DIR}/Third_Party/LwIP)

#######################################
# include paths (published to every target via ilt_platform)
#######################################
target_include_directories(ilt_platform INTERFACE
    # FreeRTOS
    ${_fr}/include
    ${_fr}/CMSIS_RTOS_V2
    ${_fr}/portable/${ILT_FREERTOS_PORT_DIR}
    # lwIP. src/include is the only one lwIP's own "lwip/xxx.h" includes need;
    # the rest are the flat paths CubeMX puts on the command line so that
    # generated code can say "opt.h" or "ethernetif.h" without a prefix.
    #
    # src/include/compat/posix* is deliberately NOT here. Those headers exist to
    # let application code call lwIP's sockets under their POSIX names, which
    # this firmware never does (LWIP_SOCKET is 0) -- and putting them on the
    # global include path makes <sys/socket.h> and <net/if.h> resolve to lwIP
    # shims instead of the system headers, which breaks any file that wants the
    # real ones. The host TAP driver is exactly such a file.
    ${_lwip}/src/include
    ${_lwip}/src/include/lwip
    ${_lwip}/src/include/lwip/apps
    ${_lwip}/src/include/lwip/priv
    ${_lwip}/src/include/lwip/prot
    ${_lwip}/src/include/netif
    ${_lwip}/src/include/netif/ppp
    ${_lwip}/system
    ${_lwip}/system/arch
)

#######################################
# sources
#######################################
# Core kernel plus the CMSIS-RTOS v2 wrapper. Explicitly NOT recursive over
# portable/: there is now more than one port in the tree (ARM_CM7 for the
# boards, Posix for the host build), and compiling two would collide on every
# vPort* symbol. The board names the one it wants in ILT_FREERTOS_PORT_DIR.
file(GLOB _freertos_sources CONFIGURE_DEPENDS
    ${_fr}/*.c
    ${_fr}/CMSIS_RTOS_V2/*.c
)

file(GLOB_RECURSE _freertos_port CONFIGURE_DEPENDS
    ${_fr}/portable/${ILT_FREERTOS_PORT_DIR}/*.c
)

if(NOT _freertos_port)
    message(FATAL_ERROR
            "No FreeRTOS port sources under portable/${ILT_FREERTOS_PORT_DIR}")
endif()

list(APPEND _freertos_sources ${_freertos_port}
                              ${_fr}/portable/MemMang/heap_4.c)

file(GLOB_RECURSE _lwip_sources CONFIGURE_DEPENDS
    ${_lwip}/src/api/*.c
    ${_lwip}/src/apps/*.c
    ${_lwip}/src/core/*.c
    ${_lwip}/src/netif/*.c
    ${_lwip}/system/OS/*.c
)

# makefsdata is a host tool that ships inside the httpd sources; it has a main()
# and is not part of the firmware. We do not use lwIP's fsdata filesystem at all
# -- ILT_OS/Lib/Net implements the fs_* API over a C++ route table instead.
list(FILTER _lwip_sources EXCLUDE REGEX "/makefsdata/")

add_library(ilt_middlewares OBJECT
    ${_freertos_sources}
    ${_lwip_sources}
)

target_link_libraries(ilt_middlewares PRIVATE ilt_platform)

# Third-party code, built as shipped: -Wall over lwIP and FreeRTOS produces a
# wall of warnings we are not going to act on, which buries warnings from our
# own code. Errors are of course still errors.
target_compile_options(ilt_middlewares PRIVATE -w)

list(LENGTH _freertos_sources _n_fr)
list(LENGTH _lwip_sources _n_lwip)
message(STATUS "  Middlewares: FreeRTOS ${_n_fr} sources (port ${ILT_FREERTOS_PORT_DIR}), lwIP ${_n_lwip} sources")
