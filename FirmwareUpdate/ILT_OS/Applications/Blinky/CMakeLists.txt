# Blinky -- blinks one of the NUCLEO-F767ZI user LEDs from an ilt::Thread.
#
# Configure-time flags. Override on the command line:
#   cmake --preset Debug -DBLINKY_LED=LD3 -DBLINKY_PERIOD_MS=100
# or from a preset's cacheVariables.

set(BLINKY_LED "LD2" CACHE STRING
    "LED to blink: LD1 (green, PB0), LD2 (blue, PB7), LD3 (red, PB14)")
set_property(CACHE BLINKY_LED PROPERTY STRINGS LD1 LD2 LD3)

set(BLINKY_PERIOD_MS "500" CACHE STRING
    "Milliseconds the LED holds each state")

set(BLINKY_STACK_BYTES "512" CACHE STRING
    "Thread stack in bytes (must be a multiple of 4)")

option(BLINKY_DOUBLE_FLASH
    "Two short flashes per period instead of an even square wave" OFF)

# Reject a bad LED here rather than letting the token paste fail later with an
# unhelpful "LD9_Pin was not declared" from the preprocessor.
if(NOT BLINKY_LED MATCHES "^LD[123]$")
    message(FATAL_ERROR "BLINKY_LED must be LD1, LD2 or LD3 (got '${BLINKY_LED}')")
endif()

# OBJECT rather than STATIC: the application is reached only through
# ILT_ApplicationStart(), and a static library would contribute nothing else the
# linker needs, so its members could be left out of the archive extraction.
add_library(ilt_app OBJECT
    BlinkyApp.cpp
)

target_include_directories(ilt_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Carries the MCU flags, include paths and USE_HAL_DRIVER/STM32F767xx, so the
# application is compiled exactly as the firmware is.
target_link_libraries(ilt_app PRIVATE ilt_platform)

target_compile_definitions(ilt_app PRIVATE
    BLINKY_LED=${BLINKY_LED}
    BLINKY_PERIOD_MS=${BLINKY_PERIOD_MS}
    BLINKY_STACK_BYTES=${BLINKY_STACK_BYTES}
    $<$<BOOL:${BLINKY_DOUBLE_FLASH}>:BLINKY_DOUBLE_FLASH>
)

message(STATUS "  Blinky: LED=${BLINKY_LED} period=${BLINKY_PERIOD_MS}ms "
               "stack=${BLINKY_STACK_BYTES}B double_flash=${BLINKY_DOUBLE_FLASH}")
