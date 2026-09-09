# ILT_TESTRIG -- the degrader and the dosimeter combined into one image.
#
#   cmake --preset Debug -DILT_APP=ILT_TESTRIG -DTESTRIG_STREAM_PORT=5000
#
# Requires more HTTP routes than the default: six are registered at start-up.

set(TESTRIG_STACK_BYTES "1536" CACHE STRING
    "Supervisor thread stack in bytes (must be a multiple of 4)")

set(TESTRIG_HOSTNAME "ilt-testrig" CACHE STRING
    "DHCP client hostname; what a router shows in its lease list")

set(TESTRIG_STREAM_PORT "5000" CACHE STRING
    "TCP port clients connect to for the live data stream")

option(TESTRIG_USE_DHCP
    "Ask for an address by DHCP, falling back to the static one below" ON)

set(TESTRIG_DHCP_TIMEOUT_MS "15000" CACHE STRING
    "How long to wait for a DHCP lease before taking the fallback address")

set(TESTRIG_FALLBACK_IP "192.168.1.200" CACHE STRING "Fallback IPv4 address")
set(TESTRIG_FALLBACK_NETMASK "255.255.255.0" CACHE STRING "Fallback netmask")
set(TESTRIG_FALLBACK_GATEWAY "192.168.1.1" CACHE STRING "Fallback gateway")

foreach(_v TESTRIG_FALLBACK_IP TESTRIG_FALLBACK_NETMASK TESTRIG_FALLBACK_GATEWAY)
    if(NOT ${_v} MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "${_v} must be a dotted quad (got '${${_v}}')")
    endif()
    # No spaces: the result goes on the compiler command line as one -D.
    string(REPLACE "." "," ${_v}_CSV "${${_v}}")
endforeach()

if(TESTRIG_STREAM_PORT LESS 1 OR TESTRIG_STREAM_PORT GREATER 65535)
    message(FATAL_ERROR "TESTRIG_STREAM_PORT must be 1..65535")
endif()

# OBJECT rather than STATIC: the application is reached only through
# ILT_ApplicationStart(), and a static library would contribute nothing else the
# linker needs, so its members could be left out of the archive extraction.
# The web UI, if it has been generated. tools/embed-website.sh builds the site
# and writes WebsiteAsset.c; without it the application falls back to the small
# built-in page, so the firmware always builds whether or not npm has been run.
set(_website_asset ${CMAKE_CURRENT_SOURCE_DIR}/WebsiteAsset.c)
if(EXISTS ${_website_asset})
    set(ILT_EMBED_WEBSITE 1)
    file(SIZE ${_website_asset} _asset_bytes)
    message(STATUS "  ILT_TESTRIG: embedding the web UI (${_asset_bytes} B of C source)")
else()
    set(ILT_EMBED_WEBSITE 0)
    set(_website_asset "")
    message(STATUS "  ILT_TESTRIG: no web UI embedded "
                   "(run tools/embed-website.sh to add it)")
endif()

add_library(ilt_app OBJECT
    TestRigApp.cpp
    Degrader.cpp
    Dosimeter.cpp
    ${_website_asset}
)

target_include_directories(ilt_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Carries the MCU flags, include paths and the chip defines, so the application
# is compiled exactly as the firmware is.
target_link_libraries(ilt_app PRIVATE ilt_platform)

target_compile_definitions(ilt_app PRIVATE
    ILT_EMBED_WEBSITE=${ILT_EMBED_WEBSITE}
    TESTRIG_STACK_BYTES=${TESTRIG_STACK_BYTES}
    TESTRIG_HOSTNAME="${TESTRIG_HOSTNAME}"
    TESTRIG_STREAM_PORT=${TESTRIG_STREAM_PORT}
    TESTRIG_USE_DHCP=$<BOOL:${TESTRIG_USE_DHCP}>
    TESTRIG_DHCP_TIMEOUT_MS=${TESTRIG_DHCP_TIMEOUT_MS}
    "TESTRIG_FALLBACK_IP=${TESTRIG_FALLBACK_IP_CSV}"
    "TESTRIG_FALLBACK_NETMASK=${TESTRIG_FALLBACK_NETMASK_CSV}"
    "TESTRIG_FALLBACK_GATEWAY=${TESTRIG_FALLBACK_GATEWAY_CSV}"
)

message(STATUS "  ILT_TESTRIG: http/80 + stream/${TESTRIG_STREAM_PORT} "
               "stack=${TESTRIG_STACK_BYTES}B dhcp=${TESTRIG_USE_DHCP}")
