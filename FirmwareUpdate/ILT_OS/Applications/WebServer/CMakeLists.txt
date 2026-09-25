# WebServer -- brings the network up and serves a status page over HTTP.
#
# Configure-time flags. Override on the command line:
#   cmake --preset Debug -DILT_APP=WebServer -DWEBSERVER_USE_DHCP=OFF
# or from a preset's cacheVariables.

set(WEBSERVER_STACK_BYTES "1536" CACHE STRING
    "Thread stack in bytes (must be a multiple of 4)")

set(WEBSERVER_HOSTNAME "ilt-degrader" CACHE STRING
    "DHCP client hostname; what a router shows in its lease list")

option(WEBSERVER_USE_DHCP
    "Ask for an address by DHCP, falling back to the static one below" ON)

set(WEBSERVER_DHCP_TIMEOUT_MS "15000" CACHE STRING
    "How long to wait for a DHCP lease before taking the fallback address")

# Used when DHCP is off, or when it is on but nothing answers in time. Written
# as dotted quads and converted to the comma-separated form the code expects.
set(WEBSERVER_FALLBACK_IP "192.168.1.200" CACHE STRING
    "Fallback IPv4 address")
set(WEBSERVER_FALLBACK_NETMASK "255.255.255.0" CACHE STRING
    "Fallback netmask")
set(WEBSERVER_FALLBACK_GATEWAY "192.168.1.1" CACHE STRING
    "Fallback gateway")

foreach(_v WEBSERVER_FALLBACK_IP WEBSERVER_FALLBACK_NETMASK WEBSERVER_FALLBACK_GATEWAY)
    if(NOT ${_v} MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "${_v} must be a dotted quad (got '${${_v}}')")
    endif()
    # No spaces: the result goes on the compiler command line as one -D.
    string(REPLACE "." "," ${_v}_CSV "${${_v}}")
endforeach()

# OBJECT rather than STATIC: the application is reached only through
# ILT_ApplicationStart(), and a static library would contribute nothing else the
# linker needs, so its members could be left out of the archive extraction.
add_library(ilt_app OBJECT
    WebServerApp.cpp
)

target_include_directories(ilt_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Carries the MCU flags, include paths and USE_HAL_DRIVER/STM32F767xx, so the
# application is compiled exactly as the firmware is.
target_link_libraries(ilt_app PRIVATE ilt_platform)

target_compile_definitions(ilt_app PRIVATE
    WEBSERVER_STACK_BYTES=${WEBSERVER_STACK_BYTES}
    WEBSERVER_HOSTNAME="${WEBSERVER_HOSTNAME}"
    WEBSERVER_USE_DHCP=$<BOOL:${WEBSERVER_USE_DHCP}>
    WEBSERVER_DHCP_TIMEOUT_MS=${WEBSERVER_DHCP_TIMEOUT_MS}
    "WEBSERVER_FALLBACK_IP=${WEBSERVER_FALLBACK_IP_CSV}"
    "WEBSERVER_FALLBACK_NETMASK=${WEBSERVER_FALLBACK_NETMASK_CSV}"
    "WEBSERVER_FALLBACK_GATEWAY=${WEBSERVER_FALLBACK_GATEWAY_CSV}"
)

if(WEBSERVER_USE_DHCP)
    message(STATUS "  WebServer: DHCP (timeout ${WEBSERVER_DHCP_TIMEOUT_MS}ms, "
                   "fallback ${WEBSERVER_FALLBACK_IP}) stack=${WEBSERVER_STACK_BYTES}B")
else()
    message(STATUS "  WebServer: static ${WEBSERVER_FALLBACK_IP} "
                   "stack=${WEBSERVER_STACK_BYTES}B")
endif()
