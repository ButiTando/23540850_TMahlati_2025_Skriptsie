/**
  ******************************************************************************
  * @file    Bsp/Degrader.h
  * @brief   The degrader's motors and limit switches, board-independent.
  *
  * The rig carries seven lenses on a carousel. A stepper rotates the carousel
  * until the wanted lens is aligned (its SELECT switch closes), then a DC motor
  * drives that lens into or out of the beam (its IN or OUT switch closes).
  *
  * Everything here is expressed in those terms rather than in ports and pins,
  * so ILT_OS/Lib/Degrader.cpp -- which holds the actual sequencing -- is the
  * same code on the DegraderDosimeter board and on a Nucleo with nothing
  * attached.
  ******************************************************************************
  */

#ifndef ILT_BSP_DEGRADER_H
#define ILT_BSP_DEGRADER_H

#include <cstddef>
#include <cstdint>

namespace bsp::degrader {

/** @brief The seven lens thicknesses, in carousel order. */
enum class Lens : std::size_t
{
    Mm2 = 0, Mm3, Mm6, Mm8, Mm10, Mm12, Mm30, Count
};

inline constexpr std::size_t kLensCount = static_cast<std::size_t>(Lens::Count);

/** @brief Which of a lens's three limit switches to read. */
enum class LimitSwitch : std::uint8_t
{
    Select, /**< Carousel is aligned on this lens. */
    In,     /**< This lens is fully in the beam. */
    Out     /**< This lens is fully out of the beam. */
};

/** @brief Carousel rotation direction. */
enum class StepperDirection : std::uint8_t { Forward, Backward };

/** @brief Which way the DC motor drives the selected lens. */
enum class BeamDirection : std::uint8_t { IntoBeam, OutOfBeam };

/**
 * @brief Whether this board actually has degrader hardware attached.
 *
 * False on a bare Nucleo. The application still runs and still answers over
 * the network; it just reports that the mechanism is absent rather than
 * pretending to drive it.
 */
bool available() noexcept;

/** @brief Power up the drivers and put both motors in a known stopped state. */
void init() noexcept;

/**
 * @brief Read one limit switch.
 *
 * Active high on this rig: true means the condition is met.
 */
bool readSwitch(Lens lens, LimitSwitch which) noexcept;

/** @brief Start rotating the carousel. Runs until stepperStop(). */
void stepperStart(StepperDirection direction) noexcept;
void stepperStop() noexcept;

/**
 * @brief Step pulses commanded since boot.
 *
 * OPEN LOOP. The A4988 has no encoder and the carousel has no absolute sensor,
 * so this counts pulses the firmware issued, not shaft movement. It is the
 * standard way a stepper is positioned, but it is an estimate: a stall or a
 * missed step accumulates as error until the next SELECT switch confirms a
 * station. Anything shown to an operator from this must be presented as
 * commanded position, and the switch-confirmed station is the one to trust.
 */
std::uint32_t stepsIssued() noexcept;

/** @brief Pulse rate the STEP output runs at, in Hz. */
std::uint32_t stepRateHz() noexcept;

/**
 * @brief Steps between one lens station and the next.
 *
 * A mechanical constant of the carousel: motor step angle, microstep setting
 * (MS1..MS3) and gear ratio together. It sets the scale of every position
 * estimate, so a wrong value makes the reported position wrong in proportion.
 */
std::uint32_t stepsPerStation() noexcept;

/** @brief Start driving the selected lens in or out. Runs until dcMotorStop(). */
void dcMotorStart(BeamDirection direction) noexcept;
void dcMotorStop() noexcept;

/**
 * @brief Stop both motors immediately.
 *
 * Called on any error or shutdown path; must be safe to call at any time,
 * including when nothing was moving.
 */
void stopAll() noexcept;

} // namespace bsp::degrader

#endif /* ILT_BSP_DEGRADER_H */
