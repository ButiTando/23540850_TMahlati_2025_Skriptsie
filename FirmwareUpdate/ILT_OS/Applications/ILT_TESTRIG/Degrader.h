/**
  ******************************************************************************
  * @file    Degrader.h
  * @brief   The degrader half of the rig: seven lenses, in or out of the beam.
  *
  * Replaces degrader_utils.c from ithemba_control_system/degrader. The original
  * ran an explicit six-state machine (lens_state_ideal -> drive_stepper_motor ->
  * read_select_switch -> drive_dc_motor -> read_on_or_off_switch ->
  * action_completed) stepped from the superloop, with busy-wait delays spelled
  * as `while (HAL_GetTick() - t < 5) {}`.
  *
  * On a thread the same sequence is just straight-line code: the waits become
  * sleeps that yield the CPU, and the states disappear because the program
  * counter already remembers where it is. The behaviour is meant to be the
  * same; what changed is that a move can no longer hang forever (see kMoveTimeoutMs)
  * and that the CPU is free while a motor runs.
  ******************************************************************************
  */

#ifndef ILT_APP_TESTRIG_DEGRADER_H
#define ILT_APP_TESTRIG_DEGRADER_H

#include "Bsp/Degrader.h"

#include "Queue.h"
#include "Thread.h"

#include <cstdint>

namespace rig {

/**
 * @brief Where one lens is.
 *
 * The numeric values are the two-bit encoding the original response word used,
 * so an existing control station reading the packed word sees what it expects.
 */
enum class LensState : std::uint8_t
{
    Out     = 0, /**< Clear of the beam. */
    In      = 1, /**< In the beam. */
    Moving  = 2, /**< Being driven; neither in nor out. */
    Unknown = 3  /**< Not yet established, e.g. before the first move. */
};

/** @brief What the motors are doing right now. */
enum class Motion : std::uint8_t
{
    Idle       = 0, /**< Nothing is being driven. */
    Selecting  = 1, /**< Stepper is rotating the carousel to a lens. */
    Inserting  = 2, /**< DC motor is driving the selected lens into the beam. */
    Retracting = 3  /**< DC motor is driving it out. */
};

/** @brief What the mechanism as a whole is doing. Also the original encoding. */
enum class ProcessStatus : std::uint8_t
{
    Processing = 0, /**< A move is in progress; new requests are queued. */
    Awake      = 1, /**< Powered but not yet homed. */
    Fault      = 2, /**< A move timed out, or there is no mechanism attached. */
    Ready      = 3  /**< Idle and able to accept a request. */
};

/**
 * @brief Drives the lens carousel on its own thread.
 *
 * Requests are queued rather than executed by the caller, so an HTTP handler
 * can ask for a configuration and return immediately -- a full seven-lens
 * change takes seconds of motor time.
 */
class Degrader : public ilt::StaticThread<1024>
{
public:
    Degrader() noexcept;

    /**
     * @brief Ask for a lens configuration.
     *
     * @param lensMask One bit per lens, bit 0 = 2 mm .. bit 6 = 30 mm. A set
     *                 bit means "in the beam".
     * @return false if the request queue is full, i.e. the rig is already well
     *         behind; the caller should retry rather than assume it was taken.
     */
    bool request(std::uint8_t lensMask) noexcept;

    /** @brief The configuration currently asked for. */
    std::uint8_t desiredMask() const noexcept { return desiredMask_; }

    /** @brief The configuration actually achieved, as far as is known. */
    std::uint8_t currentMask() const noexcept { return currentMask_; }

    LensState lensState(std::size_t lens) const noexcept;
    ProcessStatus status() const noexcept { return status_; }

    /** @brief Moves completed since boot, and moves that timed out. */
    std::uint32_t movesCompleted() const noexcept { return movesCompleted_; }
    std::uint32_t moveFailures() const noexcept { return moveFailures_; }

    /** @brief What the motors are doing at this instant. */
    Motion motion() const noexcept { return motion_; }

    /**
     * @brief The lens the carousel is being driven to, or -1 when idle.
     *
     * Index into bsp::degrader::Lens, not a thickness.
     */
    int targetLens() const noexcept { return targetLens_; }

    /**
     * @brief The last station a SELECT switch actually confirmed, or -1.
     *
     * This is the trustworthy one: it came from a sensor. Compare it with
     * positionSteps() to see how far the open-loop estimate has drifted.
     */
    int confirmedStation() const noexcept { return confirmedStation_; }

    /** @brief Step pulses commanded since boot; see bsp::degrader::stepsIssued. */
    std::uint32_t positionSteps() const noexcept;

    /**
     * @brief Commanded carousel position in units of stations, x1000.
     *
     * Fixed point rather than a float so the value crosses the API as an
     * integer: 2500 means "two and a half stations round". Wraps at the lens
     * count, because the carousel is continuous.
     */
    std::uint32_t positionMilliStations() const noexcept;

    /**
     * @brief The original firmware's packed 16-bit response word.
     *
     * Two bits per lens in carousel order, then the process status in the top
     * two bits -- byte-compatible with what the old control station parsed.
     */
    std::uint16_t responseWord() const noexcept;

protected:
    void run() override;

private:
    /** Drive one lens into or out of the beam. @return false on timeout. */
    bool moveLens(bsp::degrader::Lens lens, bool intoBeam) noexcept;

    /** Poll one switch until it closes or @p timeoutMs elapses. */
    bool awaitSwitch(bsp::degrader::Lens lens, bsp::degrader::LimitSwitch which,
                     std::uint32_t timeoutMs) noexcept;

    void reconcile() noexcept;

    /** Which station the commanded step count puts the carousel on. */
    std::size_t currentStation() const noexcept;

    ilt::Queue<std::uint8_t, 4> requests_{"degcmd"};

    volatile std::uint8_t  desiredMask_{0U};
    volatile std::uint8_t  currentMask_{0U};
    volatile ProcessStatus status_{ProcessStatus::Awake};
    volatile std::uint32_t movesCompleted_{0U};
    volatile std::uint32_t moveFailures_{0U};

    volatile Motion motion_{Motion::Idle};
    volatile int    targetLens_{-1};
    volatile int    confirmedStation_{-1};

    LensState lensStates_[bsp::degrader::kLensCount];
};

} // namespace rig

#endif /* ILT_APP_TESTRIG_DEGRADER_H */
