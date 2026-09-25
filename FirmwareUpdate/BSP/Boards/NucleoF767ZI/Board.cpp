/**
  ******************************************************************************
  * @file    Board.cpp
  * @brief   NUCLEO-F767ZI implementation of Bsp/Board.h, Led.h and Console.h.
  *
  * The CubeMX-generated main() has already configured the MPU, the clocks and
  * the peripherals by the time anything here is called; this file only maps the
  * board-neutral BSP names onto the pins and handles that main.h declares.
  ******************************************************************************
  */

#include "Bsp/Board.h"
#include "Bsp/Console.h"
#include "Bsp/Led.h"

#include "main.h"
#include "usart.h"

#include <cstdint>
#include <cstring>

namespace {

struct LedPin
{
    GPIO_TypeDef *port;
    std::uint16_t pin;
};

/* Indexed by bsp::Led. Silkscreen order: LD1 green, LD2 blue, LD3 red. */
constexpr LedPin kLeds[] = {
    {LD1_GPIO_Port, LD1_Pin}, /* Status   -- green  */
    {LD2_GPIO_Port, LD2_Pin}, /* Activity -- blue   */
    {LD3_GPIO_Port, LD3_Pin}, /* Fault    -- red    */
};

constexpr std::size_t kLedCount = sizeof(kLeds) / sizeof(kLeds[0]);

static_assert(kLedCount == static_cast<std::size_t>(bsp::Led::Count),
              "This board maps every bsp::Led; update the table if the enum grows");

/* USART3 is wired to the ST-LINK VCP on this board. Long enough that a slow
   terminal cannot wedge the caller forever, short enough to notice. */
constexpr std::uint32_t kConsoleTimeoutMs = 100U;

} // namespace

namespace bsp {

const char *boardName() noexcept
{
    return "NUCLEO-F767ZI";
}

void boardInit() noexcept
{
    /* MX_GPIO_Init() has already put the LED pins in push-pull output mode and
       driven them low, so there is nothing left to do here. The hook exists so
       boards that do need post-CubeMX setup have somewhere to put it. */
}

std::size_t ledCount() noexcept
{
    return kLedCount;
}

void ledSet(Led led, bool on) noexcept
{
    const auto index = static_cast<std::size_t>(led);
    if (index >= kLedCount)
    {
        return;
    }

    HAL_GPIO_WritePin(kLeds[index].port, kLeds[index].pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void ledToggle(Led led) noexcept
{
    const auto index = static_cast<std::size_t>(led);
    if (index >= kLedCount)
    {
        return;
    }

    HAL_GPIO_TogglePin(kLeds[index].port, kLeds[index].pin);
}

void ledAllOff() noexcept
{
    for (std::size_t i = 0; i < kLedCount; ++i)
    {
        HAL_GPIO_WritePin(kLeds[i].port, kLeds[i].pin, GPIO_PIN_RESET);
    }
}

void consoleWrite(const char *data, std::size_t length) noexcept
{
    if (data == nullptr || length == 0U)
    {
        return;
    }

    HAL_UART_Transmit(&huart3, reinterpret_cast<const std::uint8_t *>(data),
                      static_cast<std::uint16_t>(length), kConsoleTimeoutMs);
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
    consoleWrite("\r\n", 2U);
}

} // namespace bsp
