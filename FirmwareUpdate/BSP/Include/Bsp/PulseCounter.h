/**
  ******************************************************************************
  * @file    Bsp/PulseCounter.h
  * @brief   The dosimeter's six pulse-counting channels, board-independent.
  *
  * Each channel counts edges from one detector. On the real board that is a
  * hardware timer in external-clock mode, paired with a second timer to extend
  * the count to 32 bits; none of that is visible here, because the only thing
  * the application needs is "how many pulses since I last asked".
  ******************************************************************************
  */

#ifndef ILT_BSP_PULSECOUNTER_H
#define ILT_BSP_PULSECOUNTER_H

#include <cstddef>
#include <cstdint>

namespace bsp::pulse {

/** @brief Detector channels, numbered as on the schematic (PULSE_1..PULSE_6). */
inline constexpr std::size_t kChannelCount = 6U;

/** @brief Whether this board has counting hardware wired up. */
bool available() noexcept;

/** @brief Start the counters. Counts run continuously from here on. */
void init() noexcept;

/**
 * @brief Current count on one channel.
 *
 * @param channel 0..kChannelCount-1; out of range reads as 0.
 * @return Pulses counted since the last reset(), saturating rather than
 *         wrapping is NOT promised -- the hardware counter is 32 bits and will
 *         wrap, which is why the sampling period exists.
 */
std::uint32_t read(std::size_t channel) noexcept;

/** @brief Zero one channel's counter. */
void reset(std::size_t channel) noexcept;

/**
 * @brief Read all channels and zero them in one go.
 *
 * Preferred over read()+reset() per channel: it keeps the six samples as close
 * together in time as the hardware allows, so one record describes one moment
 * rather than six slightly different ones.
 *
 * @param[out] counts Array of at least kChannelCount entries.
 */
void readAndReset(std::uint32_t *counts) noexcept;

} // namespace bsp::pulse

#endif /* ILT_BSP_PULSECOUNTER_H */
