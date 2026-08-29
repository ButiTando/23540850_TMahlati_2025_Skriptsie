/**
  ******************************************************************************
  * @file    BlinkyApp.cpp
  * @brief   Blinks an LED from an ilt::Thread.
  *
  * Everything configurable here arrives as a -D from this application's
  * CMakeLists.txt; see the flags documented there.
  ******************************************************************************
  */

#include "Application.h"
#include "Thread.h"

#include "main.h" /* LDx_Pin / LDx_GPIO_Port, HAL_GPIO_* */

/* Defaults, so the translation unit still compiles if built without CMake. */
#ifndef BLINKY_LED
#define BLINKY_LED LD2
#endif
#ifndef BLINKY_PERIOD_MS
#define BLINKY_PERIOD_MS 500
#endif
#ifndef BLINKY_STACK_BYTES
#define BLINKY_STACK_BYTES 512
#endif

/* BLINKY_LED arrives as a bare token (LD2), which is pasted onto the pin and
   port macro names that CubeMX generates in main.h. Two levels of indirection
   are needed so BLINKY_LED is expanded before the ## is applied. */
#define ILT_PASTE_(a, b) a##b
#define ILT_PASTE(a, b) ILT_PASTE_(a, b)

#define BLINKY_PIN ILT_PASTE(BLINKY_LED, _Pin)
#define BLINKY_PORT ILT_PASTE(BLINKY_LED, _GPIO_Port)

namespace {

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
                HAL_GPIO_WritePin(BLINKY_PORT, BLINKY_PIN, GPIO_PIN_SET);
                sleep(BLINKY_PERIOD_MS / 8U);
                HAL_GPIO_WritePin(BLINKY_PORT, BLINKY_PIN, GPIO_PIN_RESET);
                sleep(BLINKY_PERIOD_MS / 8U);
            }
            sleep(BLINKY_PERIOD_MS);
#else
            HAL_GPIO_TogglePin(BLINKY_PORT, BLINKY_PIN);
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
