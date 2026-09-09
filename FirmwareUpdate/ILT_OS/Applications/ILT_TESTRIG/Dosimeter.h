/**
  ******************************************************************************
  * @file    Dosimeter.h
  * @brief   The dosimeter half of the rig: six pulse channels on a fixed period.
  *
  * Replaces blm_utils.c from ithemba_control_system/dosimeter. The original
  * read six timer pairs from the superloop every `period_s`, reset them, built
  * a CSV line stamped from the RTC, and pushed it down a TCP socket.
  *
  * Here that is one thread sleeping on an absolute deadline, so the sampling
  * period does not drift by however long the formatting and the network took.
  *
  * One deliberate difference: records are stamped with milliseconds since boot,
  * not a wall-clock date and time. The DegraderDosimeter schematic leaves
  * PC14/PC15 unwired, so that board has no 32 kHz crystal and therefore no
  * usable RTC -- see BSP/Boards/DegraderDosimeter/README.md. A monotonic uptime
  * that is actually correct beats a wall clock that silently is not; the
  * receiving station can stamp arrival time itself.
  ******************************************************************************
  */

#ifndef ILT_APP_TESTRIG_DOSIMETER_H
#define ILT_APP_TESTRIG_DOSIMETER_H

#include "Bsp/PulseCounter.h"

#include "Mutex.h"
#include "Thread.h"

#include <cstdint>

namespace rig {

/** @brief One sample: every channel, read at the same moment. */
struct Sample
{
    std::uint32_t sequence;                            /**< Increments per sample. */
    std::uint32_t uptimeMs;                            /**< When it was taken. */
    std::uint32_t counts[bsp::pulse::kChannelCount];   /**< Pulses this period. */
};

/**
 * @brief Samples the pulse channels and pushes each record to the stream.
 *
 * The period is settable at run time, as the original's SET_PERIOD command was,
 * and is clamped to the same 1..3600 second range.
 */
class Dosimeter : public ilt::StaticThread<1024>
{
public:
    static constexpr std::uint32_t kMinPeriodS = 1U;
    static constexpr std::uint32_t kMaxPeriodS = 3600U;

    Dosimeter() noexcept;

    /**
     * @brief Change the sampling period.
     * @return false if @p seconds is outside kMinPeriodS..kMaxPeriodS, in which
     *         case the period is unchanged.
     */
    bool setPeriod(std::uint32_t seconds) noexcept;

    std::uint32_t period() const noexcept { return periodS_; }

    /** @brief Copy the most recent sample. Safe from any thread. */
    void latest(Sample &out) const noexcept;

    std::uint32_t sampleCount() const noexcept { return sequence_; }

protected:
    void run() override;

private:
    volatile std::uint32_t periodS_{1U};
    volatile std::uint32_t sequence_{0U};

    /* Written by this thread, read by HTTP handlers on tcpip_thread. Guarded
       by a mutex rather than a lock-free scheme: the critical section is a
       32-byte copy a few times a second, so the contention it can cause is far
       cheaper than the cost of getting a hand-rolled seqlock subtly wrong. */
    mutable ilt::Mutex latestLock_{"dosim"};
    Sample             latest_{};
};

} // namespace rig

#endif /* ILT_APP_TESTRIG_DOSIMETER_H */
