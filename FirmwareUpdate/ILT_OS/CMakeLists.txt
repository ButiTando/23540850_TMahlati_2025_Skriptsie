# The portable layer, and the choice of application.
#
# Nothing under Lib/ or Applications/ may include a chip header. What they need
# from the hardware comes from BSP/Include/Bsp/*.h, whose include path arrives
# on ilt_platform; that is the rule that lets the same application build for the
# F767 Nucleo today and the H723 board later.

#######################################
# ilt_os: threads and networking
#######################################
set(ILT_HTTP_MAX_ROUTES "12" CACHE STRING
    "Maximum pages HttpServer can serve")
set(ILT_HTTP_RENDER_BUFFERS "2" CACHE STRING
    "Concurrent dynamic-page renders; one buffer is held per in-flight request")
set(ILT_HTTP_RENDER_BYTES "1024" CACHE STRING
    "Bytes a dynamic page may render into")

set(ILT_STREAM_MAX_CLIENTS "4" CACHE STRING
    "Simultaneous clients on the live data stream")
set(ILT_STREAM_RECORD_BYTES "192" CACHE STRING
    "Longest single stream record, including the newline")
set(ILT_STREAM_POOL_DEPTH "4" CACHE STRING
    "Records that may be in flight to tcpip_thread at once")

add_library(ilt_os OBJECT
    Lib/Thread.cpp
    Lib/Kernel.cpp
    Lib/Mutex.cpp
    Lib/Semaphore.cpp
    Lib/Queue.cpp
    Lib/EventFlags.cpp
    Lib/Timer.cpp
    Lib/Net/NetworkStack.cpp
    Lib/Net/HttpServer.cpp
    Lib/Net/StreamServer.cpp
)

target_link_libraries(ilt_os PRIVATE ilt_platform)

target_compile_definitions(ilt_os PRIVATE
    ILT_HTTP_MAX_ROUTES=${ILT_HTTP_MAX_ROUTES}
    ILT_HTTP_RENDER_BUFFERS=${ILT_HTTP_RENDER_BUFFERS}
    ILT_HTTP_RENDER_BYTES=${ILT_HTTP_RENDER_BYTES}
    ILT_STREAM_MAX_CLIENTS=${ILT_STREAM_MAX_CLIENTS}
    ILT_STREAM_RECORD_BYTES=${ILT_STREAM_RECORD_BYTES}
    ILT_STREAM_POOL_DEPTH=${ILT_STREAM_POOL_DEPTH}
)

# On ilt_platform rather than on ilt_os so the application can include
# "Net/HttpServer.h" without linking against ilt_os directly.
target_include_directories(ilt_platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Lib)

math(EXPR _render_total "${ILT_HTTP_RENDER_BUFFERS} * ${ILT_HTTP_RENDER_BYTES}")
message(STATUS "  ILT_OS: ${ILT_HTTP_MAX_ROUTES} routes, "
               "${ILT_HTTP_RENDER_BUFFERS} x ${ILT_HTTP_RENDER_BYTES}B render pool "
               "(${_render_total} B of .bss)")

#######################################
# application selection
#######################################
# Exactly one application from ILT_OS/Applications is built into the image.
# Its own CMakeLists.txt declares the flags that shape it, e.g.
#   cmake --preset Debug -DILT_APP=Blinky -DBLINKY_LED=Fault
file(GLOB _app_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/Applications
     ${CMAKE_CURRENT_SOURCE_DIR}/Applications/*)

set(_apps "")
foreach(_d ${_app_dirs})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Applications/${_d}/CMakeLists.txt)
        list(APPEND _apps ${_d})
    endif()
endforeach()

set(ILT_APP "WebServer" CACHE STRING
    "Application under ILT_OS/Applications to build into the image")
set_property(CACHE ILT_APP PROPERTY STRINGS ${_apps})

if(NOT ILT_APP IN_LIST _apps)
    message(FATAL_ERROR "ILT_APP='${ILT_APP}' not found. Available: ${_apps}")
endif()

add_subdirectory(Applications/${ILT_APP})
