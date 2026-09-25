/**
  ******************************************************************************
  * @file    Kernel.cpp
  * @brief   Scheduler-wide queries and critical sections.
  ******************************************************************************
  */

#include "Kernel.h"

namespace ilt {
namespace kernel {

std::uint32_t tickCount() noexcept
{
    return osKernelGetTickCount();
}

std::uint32_t tickFrequency() noexcept
{
    return osKernelGetTickFreq();
}

std::uint32_t msToTicks(std::uint32_t milliseconds) noexcept
{
    if (milliseconds == kWaitForever || milliseconds == 0U)
    {
        return milliseconds;
    }

    const std::uint32_t frequency = osKernelGetTickFreq();
    if (frequency == 1000U)
    {
        return milliseconds; /* the common case, and exact */
    }

    /* 64-bit intermediate: at 1 kHz a 32-bit product overflows after only ~4
       seconds of requested delay. */
    const std::uint64_t ticks =
        (static_cast<std::uint64_t>(milliseconds) * frequency) / 1000ULL;

    if (ticks == 0U)
    {
        return 1U; /* round up rather than turn a wait into a poll */
    }

    /* Saturate instead of wrapping; kWaitForever is reserved, so stop below it. */
    if (ticks >= kWaitForever)
    {
        return kWaitForever - 1U;
    }

    return static_cast<std::uint32_t>(ticks);
}

bool inIsr() noexcept
{
#if defined(ILT_HOST_BUILD)
    /* A Linux process has no interrupt context. Not a stub standing in for
       something unimplemented -- it is the correct answer here, and it is why
       the FreeRTOS POSIX port provides no equivalent test to call. */
    return false;
#else
    /* CMSIS has no public "am I in an ISR" call, and the architectural test
       (__get_IPSR() != 0) lives in CMSIS-Core -- a header nothing above BSP/ is
       allowed to include. The Cortex-M FreeRTOS port exposes the same check
       through portmacro.h, which is already in scope via FreeRTOS.h and carries
       no chip dependency, so the rule holds and the answer is identical. */
    return xPortIsInsideInterrupt() != pdFALSE;
#endif
}

bool isRunning() noexcept
{
    return osKernelGetState() == osKernelRunning;
}

void delay(std::uint32_t milliseconds) noexcept
{
    osDelay(msToTicks(milliseconds));
}

bool delayUntil(std::uint32_t tick) noexcept
{
    return osDelayUntil(tick) == osOK;
}

} // namespace kernel

SchedulerLock::SchedulerLock() noexcept
    : previous_(osKernelLock())
{
}

SchedulerLock::~SchedulerLock()
{
    /* Restores the previous lock state rather than blindly unlocking, so
       nesting works: an inner scope ending does not resume the scheduler while
       an outer scope still wants it suspended. */
    if (previous_ >= 0)
    {
        osKernelRestoreLock(previous_);
    }
}

InterruptLock::InterruptLock() noexcept
    : savedMask_(0U), fromIsr_(kernel::inIsr())
{
    if (fromIsr_)
    {
        savedMask_ = taskENTER_CRITICAL_FROM_ISR();
    }
    else
    {
        taskENTER_CRITICAL();
    }
}

InterruptLock::~InterruptLock()
{
    if (fromIsr_)
    {
        taskEXIT_CRITICAL_FROM_ISR(savedMask_);
    }
    else
    {
        taskEXIT_CRITICAL();
    }
}

} // namespace ilt
