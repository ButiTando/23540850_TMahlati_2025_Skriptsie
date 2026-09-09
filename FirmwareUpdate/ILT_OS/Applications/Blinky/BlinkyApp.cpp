/**
  ******************************************************************************
  * @file    BlinkyApp.cpp
  * @brief   Blinks a board LED from an ilt::Thread.
  *
  * The smoke test: if this blinks, the clocks, the scheduler and the BSP are
  * all working. It names the LED by role rather than by port and pin, so it
  * builds unchanged for any board under BSP/Boards.
  *
  * Everything configurable here arrives as a -D from this application's
  * CMakeLists.txt; see the flags documented there.
  ******************************************************************************
  */

#include "Application.h"
#include "Thread.h"

#include "Bsp/Led.h"

/* Defaults, so the translation unit still compiles if built without CMake. */
#ifndef BLINKY_LED
#define BLINKY_LED Status
#endif
#ifndef BLINKY_PERIOD_MS
#define BLINKY_PERIOD_MS 500
#endif
#ifndef BLINKY_STACK_BYTES
#define BLINKY_STACK_BYTES 512
#endif

namespace {

constexpr bsp::Led kLed = bsp::Led::BLINKY_LED;

class BlinkyThread : public ilt::StaticThread<BLINKY_STACK_BYTES>
{
public:
    BlinkyThread() noexcept
        : StaticThread("blinky", osPriorityNormal)
    {
    }

protected:
    void run() override
    {
        for (;;)
        {
#ifdef BLINKY_DOUBLE_FLASH
            /* Two short pulses, then hold off for the rest of the period. */
            for (int i = 0; i < 2; ++i)
            {
                bsp::ledSet(kLed, true);
                sleep(BLINKY_PERIOD_MS / 8U);
                bsp::ledSet(kLed, false);
                sleep(BLINKY_PERIOD_MS / 8U);
            }
            sleep(BLINKY_PERIOD_MS);
#else
            bsp::ledToggle(kLed);
            sleep(BLINKY_PERIOD_MS);
#endif
        }
    }
};

/* Constructed by __libc_init_array before main(); only the stack and control
   block are reserved at this point, no RTOS call happens yet. */
BlinkyThread g_blinky;

} // namespace

extern "C" void ILT_ApplicationStart(void)
{
    g_blinky.start();
}
