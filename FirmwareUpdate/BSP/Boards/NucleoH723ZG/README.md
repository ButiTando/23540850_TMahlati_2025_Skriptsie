# NucleoH723ZG (placeholder)

> The production H723 target is `../DegraderDosimeter`, which now carries the
> pin allocation from `Schematics/DegraderDosimeter`. Keep this board only if
> you still want to run firmware on a bare NUCLEO-H723ZG for bench testing --
> the existing dosimeter project targets one. The two share the MCU, so most of
> the work below is common to both.

Not buildable yet. There is deliberately no `CMakeLists.txt` here, so
`BSP/CMakeLists.txt` does not offer this directory as a value for `ILT_BOARD`
and a typo cannot half-configure it.

To add the board, this directory needs:

| What | Where it comes from |
| --- | --- |
| `Core/`, `Drivers/`, `LWIP/Target/`, `startup_stm32h723xx.s`, `*.ld`, `*.ioc` | A CubeMX project for the H723 with FreeRTOS + LwIP enabled |
| `CMakeLists.txt` | Copy the F767 one; change the chip define to `STM32H723xx`, the HAL driver paths to `STM32H7xx_HAL_Driver`, and the startup/linker script names |
| `Board.cpp` | Implements `Bsp/Board.h`, `Bsp/Led.h`, `Bsp/Console.h` for this board's LEDs and VCP UART |
| `BoardEthernet.cpp` | Implements `Bsp/Ethernet.h`; the H723 Nucleo also carries a LAN8742A, so this is close to the F767 version |

Nothing above `BSP/` should need editing: `ILT_OS/Lib` and every application
already reach the hardware only through `BSP/Include/Bsp/*.h`.

Two things that will differ and are worth getting right the first time:

* **MCU flags.** The H723 is Cortex-M7 with the double-precision FPU like the
  F767, so `-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard` carries over.
* **Where the Ethernet DMA can reach.** This is the F767 issue in a different
  costume. On the H7 the ETH DMA lives in domain D2 and cannot see DTCM *or*
  AXI SRAM in D1, so the descriptors and the lwIP RX pool must be placed in D2
  SRAM (`0x30000000`), and the MPU must mark that region non-cacheable or
  device memory. See the memory-region comments in the F767 linker script for
  the shape of the fix.
