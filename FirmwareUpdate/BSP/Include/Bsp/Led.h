/**
  ******************************************************************************
  * @file    Bsp/Led.h
  * @brief   Board-independent access to the user LEDs.
  *
  * Application code names an LED by its role, never by a port and pin, so the
  * same application builds for any board in BSP/Boards. A board that has fewer
  * LEDs than the enum lists still accepts every id; the ones it cannot drive
  * are ignored rather than being a compile error, so an application does not
  * have to be conditionalised per board.
  ******************************************************************************
  */

#ifndef ILT_BSP_LED_H
#define ILT_BSP_LED_H

#include <cstddef>

namespace bsp {

/**
 * @brief The user LEDs, named by role rather than by colour position.
 *
 * Status/Activity/Fault are the roles firmware actually reasons about; the
 * colour each maps to is the board's business. On the NUCLEO boards these are
 * the three user LEDs in silkscreen order (LD1/LD2/LD3).
 */
enum class Led : std::size_t
{
    Status = 0, /**< Heartbeat -- the firmware is alive and scheduling. */
    Activity,   /**< Something happened: a request served, a packet handled. */
    Fault,      /**< A failure the operator should see. */
    Count
};

/** @brief Number of LEDs this board can actually drive (may be < Led::Count). */
std::size_t ledCount() noexcept;

/** @brief Drive one LED. Ids at or beyond ledCount() are silently ignored. */
void ledSet(Led led, bool on) noexcept;

/** @brief Invert one LED. Ids at or beyond ledCount() are silently ignored. */
void ledToggle(Led led) noexcept;

/** @brief Turn every LED on this board off. */
void ledAllOff() noexcept;

} // namespace bsp

#endif /* ILT_BSP_LED_H */
