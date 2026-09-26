# Board selection.
#
# Every directory under Boards/ that has a CMakeLists.txt is a candidate. The
# chosen one is responsible for:
#   * filling ilt_platform with the MCU flags, chip defines and include paths,
#   * defining the ilt_bsp OBJECT library (CubeMX tree, HAL, BSP headers impl),
#   * exporting ILT_STARTUP_SOURCE, ILT_LINKER_SCRIPT and ILT_FREERTOS_PORT_DIR.

file(GLOB _board_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/Boards
     ${CMAKE_CURRENT_SOURCE_DIR}/Boards/*)

set(_boards "")
foreach(_d ${_board_dirs})
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Boards/${_d}/CMakeLists.txt)
        list(APPEND _boards ${_d})
    endif()
endforeach()

# The cache entry itself is created at the top level, which has to know whether
# this is a cross build before project() runs. Here we only publish the choices
# and check the one that was made.
set_property(CACHE ILT_BOARD PROPERTY STRINGS ${_boards})

if(NOT ILT_BOARD IN_LIST _boards)
    message(FATAL_ERROR
            "ILT_BOARD='${ILT_BOARD}' has no BSP/Boards/${ILT_BOARD}/CMakeLists.txt. "
            "Available: ${_boards}")
endif()

# The board-neutral BSP headers. On ilt_platform rather than on ilt_bsp so that
# the application and ILT_OS/Lib can include <Bsp/Led.h> without depending on
# the board target directly.
target_include_directories(ilt_platform INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Include)

# Replace the board's real rig hardware with a software model.
#
# Off by default on hardware: a simulation that invents switch transitions lets
# the lens state machine appear to work on a rig that cannot move, and the first
# honest failure would then be on the real one. Turn it on deliberately to
# develop the web UI against a board with nothing attached.
option(ILT_SIMULATE_RIG
       "Simulate the degrader and dosimeter instead of driving real hardware" OFF)

# Anything above the BSP that reports on the rig needs to be able to say the
# readings are synthetic; "present" alone would be a lie the UI repeats.
if(ILT_SIMULATE_RIG)
    target_compile_definitions(ilt_platform INTERFACE ILT_RIG_SIMULATED=1)
else()
    target_compile_definitions(ilt_platform INTERFACE ILT_RIG_SIMULATED=0)
endif()

set(ILT_RIG_SIMULATION_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Shared/RigSimulation.cpp
    CACHE INTERNAL "Board-neutral rig simulation")

add_subdirectory(Boards/${ILT_BOARD})
