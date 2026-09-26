/**
  ******************************************************************************
  * @file    Kernel.h
  * @brief   Scheduler-wide queries and the two kinds of critical section.
  *
  * The pieces of the RTOS that are not an object: what time is it, are we in an
  * interrupt, and how to keep something else from running for a moment.
  *
  * Every timeout in ilt is expressed in MILLISECONDS, not kernel ticks. CMSIS
  * counts in ticks, and the two happen to be the same here because
  * configTICK_RATE_HZ is 1000 -- but only happen to be. Passing milliseconds
  * through msToTicks() means a change to the tick rate cannot silently turn
  * every timeout in the firmware into the wrong duration.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_KERNEL_H
#define ILT_OS_LIB_KERNEL_H

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "task.h"

#include <cstdint>

namespace ilt {

/**
 * @brief Block until the operation succeeds, however long that takes.
 *
 * Numerically osWaitForever, and passed through untouched by msToTicks() --
 * it is a sentinel, not a duration.
 */
inline constexpr std::uint32_t kWaitForever = 0xFFFFFFFFU;

/** @brief Do not block at all: succeed now or fail now. */
inline constexpr std::uint32_t kNoWait = 0U;

namespace kernel {

/** @brief Milliseconds since the scheduler started. Wraps after ~49 days. */
std::uint32_t tickCount() noexcept;

/** @brief Kernel ticks per second (configTICK_RATE_HZ). */
std::uint32_t tickFrequency() noexcept;

/**
 * @brief Convert a millisecond timeout to the kernel ticks CMSIS wants.
 *
 * kWaitForever passes through unchanged. Any other non-zero duration that would
 * round down to zero ticks is rounded up to one instead: a caller asking to
 * wait a short time meant to wait, and returning "timed out" instantly is the
 * one answer they did not ask for.
 */
std::uint32_t msToTicks(std::uint32_t milliseconds) noexcept;

/**
 * @brief Whether the caller is inside an interrupt handler.
 *
 * Worth checking before an RTOS call in code that runs in both contexts: most
 * of the blocking API is illegal from an ISR, and CMSIS answers osErrorISR
 * rather than trapping.
 */
bool inIsr() noexcept;

/** @brief Whether osKernelStart() has been called and the scheduler is running. */
bool isRunning() noexcept;

/** @brief Block the calling thread. Illegal from an ISR. */
void delay(std::uint32_t milliseconds) noexcept;

/**
 * @brief Sleep until an absolute tick, for drift-free periodic work.
 *
 * Unlike delay(), the period does not stretch by however long the thread's own
 * work took:
 *
 *     uint32_t next = ilt::kernel::tickCount();
 *     for (;;) {
 *         doWork();
 *         next += 100U;
 *         ilt::kernel::delayUntil(next);
 *     }
 *
 * @return false if @p tick is already in the past, in which case the call does
 *         not block and the caller has overrun its period.
 */
bool delayUntil(std::uint32_t tick) noexcept;

} // namespace kernel

/**
 * @brief Stops other THREADS running, but leaves interrupts on.
 *
 * The cheap way to make a multi-step change to state that only threads touch.
 * Interrupts still run, so this does NOT protect against an ISR -- use
 * InterruptLock for that.
 *
 * Nothing that blocks may be called while this is held: with the scheduler
 * suspended there is no other thread to switch to, and the system stops dead.
 */
class SchedulerLock
{
public:
    SchedulerLock() noexcept;
    ~SchedulerLock();

    SchedulerLock(const SchedulerLock &)            = delete;
    SchedulerLock &operator=(const SchedulerLock &) = delete;

private:
    std::int32_t previous_;
};

/**
 * @brief Stops interrupts as well as threads.
 *
 * The only thing that makes a section atomic with respect to an ISR. Keep it to
 * a handful of instructions: interrupts are masked for the whole scope, which
 * directly adds to worst-case interrupt latency -- including the Ethernet RX
 * interrupt, which drops frames if it is kept waiting.
 *
 * Safe to use from an ISR as well as a thread; nesting is handled by FreeRTOS.
 * Nothing that blocks may be called while this is held.
 */
class InterruptLock
{
public:
    InterruptLock() noexcept;
    ~InterruptLock();

    InterruptLock(const InterruptLock &)            = delete;
    InterruptLock &operator=(const InterruptLock &) = delete;

private:
    UBaseType_t savedMask_;
    bool        fromIsr_;
};

} // namespace ilt

#endif /* ILT_OS_LIB_KERNEL_H */
