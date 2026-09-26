/**
  ******************************************************************************
  * @file    Bsp/Board.h
  * @brief   Board identity and the one-time bring-up hook.
  *
  * Clocks, the MPU and the CubeMX peripheral inits still run from the
  * generated main(); this header covers what portable code legitimately wants
  * to know or do afterwards.
  ******************************************************************************
  */

#ifndef ILT_BSP_BOARD_H
#define ILT_BSP_BOARD_H

namespace bsp {

/**
 * @brief Human-readable board name, e.g. "NUCLEO-F767ZI".
 *
 * Points at flash and is valid for the life of the program.
 */
const char *boardName() noexcept;

/**
 * @brief Board-level init that must happen after the CubeMX peripheral inits
 *        but before any application thread runs.
 *
 * Called from ILT_ApplicationStart()'s caller side, i.e. once, from main(),
 * with the scheduler initialised but not started. Safe to call when the board
 * has nothing extra to do -- the default is a no-op.
 */
void boardInit() noexcept;

} // namespace bsp

#endif /* ILT_BSP_BOARD_H */
