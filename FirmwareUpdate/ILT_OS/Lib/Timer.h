/**
  ******************************************************************************
  * @file    Timer.h
  * @brief   Software timers, run by the RTOS timer service thread.
  *
  * Derive, override onExpire(), then start():
  *
  *     class Watchdog : public ilt::Timer {
  *     public:
  *         Watchdog() : Timer(ilt::Timer::Mode::Periodic, "wdog") {}
  *     protected:
  *         void onExpire() override { bsp::ledToggle(bsp::Led::Fault); }
  *     };
  *
  *     static Watchdog watchdog;
  *     watchdog.start(1000);
  *
  * These are NOT hardware timers. onExpire() runs on the shared FreeRTOS timer
  * service thread, so two things follow. It must not block -- a callback that
  * waits delays every other software timer in the system. And its timing is
  * only as good as that thread's priority lets it be; anything needing real
  * precision belongs on a hardware timer via the BSP, not here.
  *
  * The control block lives inside the object, so a Timer costs no FreeRTOS heap.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_TIMER_H
#define ILT_OS_LIB_TIMER_H

#include "Kernel.h"

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "timers.h"

namespace ilt {

class Timer
{
public:
    enum class Mode
    {
        OneShot,  /**< Fires once, then stops. */
        Periodic  /**< Re-arms itself until stop(). */
    };

    /**
     * @param mode Whether the timer repeats.
     * @param name Optional debugger label, stored by pointer.
     */
    explicit Timer(Mode mode = Mode::OneShot, const char *name = nullptr) noexcept;

    virtual ~Timer();

    Timer(const Timer &)            = delete;
    Timer &operator=(const Timer &) = delete;
    Timer(Timer &&)                 = delete;
    Timer &operator=(Timer &&)      = delete;

    bool isValid() const noexcept { return handle_ != nullptr; }

    /**
     * @brief Arm the timer.
     *
     * Safe to call on a running timer: the period is replaced and the countdown
     * restarts from now.
     *
     * @param periodMs Time to the next expiry. Must not be zero.
     * @return false if the timer could not be armed.
     */
    bool start(std::uint32_t periodMs) noexcept;

    /** @brief Disarm. Harmless if it was not running. */
    bool stop() noexcept;

    bool isRunning() const noexcept;

    osTimerId_t handle() const noexcept { return handle_; }

protected:
    /**
     * @brief Called on the timer service thread each time the period elapses.
     *
     * Must not block. Must not be relied on for precise timing.
     */
    virtual void onExpire() = 0;

private:
    /**
     * osTimerNew() takes a plain function pointer, which a non-static member
     * function cannot provide, so this static shim recovers the object from the
     * argument and dispatches to onExpire().
     */
    static void trampoline(void *argument);

    osTimerAttr_t  attr_{};
    StaticTimer_t  controlBlock_{};
    osTimerId_t    handle_{nullptr};
};

} // namespace ilt

#endif /* ILT_OS_LIB_TIMER_H */
