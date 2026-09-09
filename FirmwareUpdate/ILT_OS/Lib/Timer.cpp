/**
  ******************************************************************************
  * @file    Timer.cpp
  * @brief   Software timers.
  ******************************************************************************
  */

#include "Timer.h"

namespace ilt {

Timer::Timer(Mode mode, const char *name) noexcept
{
    attr_.name    = name;
    attr_.cb_mem  = &controlBlock_;
    attr_.cb_size = sizeof(controlBlock_);

    /* Creating the timer here is safe even though the vtable is still Timer's:
       osTimerNew() only registers it, and nothing can call trampoline() until
       start(), which the derived object cannot reach until its own constructor
       has finished. Contrast ilt::Thread, where the task must be created in
       start() precisely because it would otherwise begin running immediately. */
    handle_ = osTimerNew(&Timer::trampoline,
                         mode == Mode::Periodic ? osTimerPeriodic : osTimerOnce,
                         this, &attr_);
}

Timer::~Timer()
{
    if (handle_ != nullptr)
    {
        /* Stop before delete: the service thread must not be part-way through
           dispatching onExpire() on an object whose vtable is being torn down. */
        osTimerStop(handle_);
        osTimerDelete(handle_);
        handle_ = nullptr;
    }
}

bool Timer::start(std::uint32_t periodMs) noexcept
{
    if (handle_ == nullptr || periodMs == 0U)
    {
        return false;
    }

    return osTimerStart(handle_, kernel::msToTicks(periodMs)) == osOK;
}

bool Timer::stop() noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osTimerStop(handle_) == osOK;
}

bool Timer::isRunning() const noexcept
{
    return handle_ != nullptr && osTimerIsRunning(handle_) != 0U;
}

void Timer::trampoline(void *argument)
{
    static_cast<Timer *>(argument)->onExpire();
}

} // namespace ilt
