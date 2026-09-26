/**
  ******************************************************************************
  * @file    Board.cpp
  * @brief   Host implementation of Bsp/Board.h, Led.h and Console.h.
  *
  * The "board" is a Linux process. LEDs become one line of output each time
  * they change -- printing every toggle of a 2 Hz heartbeat would bury
  * everything else, so only transitions are reported.
  ******************************************************************************
  */

#include "Bsp/Board.h"
#include "Bsp/Console.h"
#include "Bsp/Led.h"

#include "cmsis_os2.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

const char *const kLedNames[] = {"STATUS", "ACTIVITY", "FAULT"};
constexpr std::size_t kLedCount = sizeof(kLedNames) / sizeof(kLedNames[0]);

static_assert(kLedCount == static_cast<std::size_t>(bsp::Led::Count),
              "LED name table must cover every bsp::Led");

bool g_ledState[kLedCount] = {false, false, false};

} // namespace

namespace bsp {

const char *boardName() noexcept
{
    return "Host (POSIX/TAP)";
}

void boardInit() noexcept
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0); /* line buffered: no lost output */
}

std::size_t ledCount() noexcept { return kLedCount; }

void ledSet(Led led, bool on) noexcept
{
    const auto index = static_cast<std::size_t>(led);
    if (index >= kLedCount || g_ledState[index] == on)
    {
        return;
    }

    g_ledState[index] = on;
    std::printf("[led] %-8s %s\n", kLedNames[index], on ? "on" : "off");
}

void ledToggle(Led led) noexcept
{
    const auto index = static_cast<std::size_t>(led);
    if (index < kLedCount)
    {
        ledSet(led, !g_ledState[index]);
    }
}

void ledAllOff() noexcept
{
    for (std::size_t i = 0; i < kLedCount; ++i)
    {
        ledSet(static_cast<Led>(i), false);
    }
}

void consoleWrite(const char *data, std::size_t length) noexcept
{
    if (data != nullptr && length > 0U)
    {
        std::fwrite(data, 1, length, stdout);
    }
}

void consoleWrite(const char *text) noexcept
{
    if (text != nullptr)
    {
        consoleWrite(text, std::strlen(text));
    }
}

void consoleWriteLine(const char *text) noexcept
{
    consoleWrite(text);
    consoleWrite("\n", 1U);
}

} // namespace bsp

/**
 * @brief lwIP's millisecond clock.
 *
 * Needed whenever LWIP_TIMERS is on, in OS mode as well as bare metal, and the
 * CMSIS-RTOS sys_arch.c does not provide it. On the MCU boards ethernetif.c
 * supplies it from HAL_GetTick(); here the RTOS tick is the same thing.
 */
extern "C" std::uint32_t sys_now(void)
{
    return osKernelGetTickCount();
}
