/**
  ******************************************************************************
  * @file    Bsp/Console.h
  * @brief   Board-independent debug console (blocking, line-oriented).
  *
  * On the NUCLEO boards this is the ST-LINK virtual COM port, so a host only
  * needs a serial terminal on the same USB cable used for flashing. A board
  * without a console links a stub, so callers never need to guard.
  *
  * Blocking and unbuffered: intended for bring-up messages such as the address
  * DHCP handed out, not for a data path.
  ******************************************************************************
  */

#ifndef ILT_BSP_CONSOLE_H
#define ILT_BSP_CONSOLE_H

#include <cstddef>

namespace bsp {

/** @brief Write raw bytes. Blocks until the UART has accepted them. */
void consoleWrite(const char *data, std::size_t length) noexcept;

/** @brief Write a NUL-terminated string. */
void consoleWrite(const char *text) noexcept;

/** @brief Write a string followed by CRLF. */
void consoleWriteLine(const char *text) noexcept;

} // namespace bsp

#endif /* ILT_BSP_CONSOLE_H */
