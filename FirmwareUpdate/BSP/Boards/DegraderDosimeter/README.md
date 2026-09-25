# DegraderDosimeter (STM32H723ZGTx)

The custom board from `Schematics/DegraderDosimeter`. It merges the two existing
rigs onto one MCU: the degrader (plate switches, stepper, DC motor — previously
`ithemba_control_system/degrader`, an STM32F746) and the dosimeter (six pulse
counters — previously `ithemba_control_system/dosimeter`, already an H723).

`DegraderDosimeter.ioc` carries the pin allocation taken directly from the
schematic. It was produced by exporting the KiCad netlist, reading U4's 144 pins,
and cross-checking every one against the `.ioc`: 52 pins carry a real net in the
schematic and all 52 are configured, with nothing extra invented.

There is **no `CMakeLists.txt` here yet**, so `ILT_BOARD` will not offer this
board until the CubeMX tree is generated (see "Before this builds" below).

## Pin allocation

| Function | Pins |
| --- | --- |
| Ethernet RMII | PA1 REF_CLK, PA2 MDIO, PA7 CRS_DV, PB13 TXD1, PC1 MDC, PC4 RXD0, PC5 RXD1, PG11 TX_EN, PG13 TXD0 |
| PHY | LAN8742A (U1), 25 MHz crystal Y1 — same part as the NUCLEO boards |
| Debug | PA13 SWDIO, PA14 SWCLK, PB3 SWO |
| Clock | PH0/PH1 — HSE crystal Y2 |
| Pulse inputs | PA5 TIM2_CH1_ETR, PA0 TIM5_CH1, PF0 TIM23_CH1, PF12 TIM24_CH2, PA6 TIM3_CH1, PD12 TIM4_CH1 |
| Stepper (A4988) | PB14 STEP via TIM12_CH1, PC2_C DIR, PB2 nRESET, PB6 nSLEEP, PB7 nENABLE, PD14/PD15/PD13 MS1/MS2/MS3 |
| DC motor | PB5 EN, PC7 DIR1, PC6 DIR2 |
| Plate switches | 7 sizes x {LEFT, RIGHT, SELECTED}, all GPIO input with pull-down — see the table below |

Switches, by plate size:

| Size | LEFT | RIGHT | SELECTED |
| --- | --- | --- | --- |
| 2 mm | PD1 | PE5 | PD3 |
| 3 mm | PA3 | PE2 | PF8 |
| 6 mm | PD7 | PE4 | PF3 |
| 8 mm | PF5 | PE6 | PF7 |
| 10 mm | PD0 | PD4 | PF2 |
| 12 mm | PF10 | PD6 | PC0 |
| 30 mm | PE3 | PD5 | PC3_C |

The pulse-input timer channels are deliberately identical to the ones the
existing BLM firmware already uses, so that code ports across unchanged.

### Net names were renamed to be legal C

The schematic names the switch nets `2mm_LEFT_SWITCH`, `30mm_SELECTED_SWITCH`
and so on. CubeMX turns a `GPIO_Label` into `#define <label>_Pin`, and an
identifier cannot start with a digit — those would not compile. They are
`SW_<SIZE>_<ROLE>` in the `.ioc` (`SW_2MM_LEFT`, `SW_30MM_SELECTED`).

Likewise KiCad's active-low markers `!RESET`, `!SLEEP`, `!ENABLE` became
`STEP_NRESET`, `STEP_NSLEEP`, `STEP_NENABLE`.

All three stepper control lines are active low and are configured to idle
**high**: reset released, not asleep, and the driver **disabled**, so the motor
cannot move between reset and the firmware taking control.

## The two designs disagree about TIM12

Merging the rigs creates one real conflict, which the pin allocation settles but
which is worth knowing about:

* The **degrader** drives the stepper STEP line from **TIM12 CH1 PWM** on PB14
  (Period `500-1`, Prescaler `1080-1`, Pulse `100-1`).
* The **dosimeter** used TIM12 with no pins at all, as a gating timebase
  (`TIM_TRGO_RESET`) for its counting chain.

PB14 is wired to STEP on this board, so TIM12 takes the degrader's PWM role and
the `.ioc` carries the degrader's settings verbatim. **The dosimeter's gating
timebase has nowhere to live and must be moved to a spare timer** — TIM6, TIM8,
TIM13-TIM17 are all free. Check what the BLM firmware actually did with TIM12
before porting it.

Similarly, the degrader used TIM2 and TIM4 as internal (pin-less) timebases,
while here both are pulse counters driven from PA5 and PD12. Those two degrader
timebases also need rehoming.

The six pulse timers each carry `VP_TIM<n>_VS_ControllerModeClock = Clock Mode`,
which is what puts them in external-clock counter mode; without it they would be
ordinary up-counters on the internal clock and would count nothing.

One deviation from the dosimeter: it took TIM24's external clock from channel 1
(PF11), but **PF11 is not routed on this board**, so PULSE_4 uses channel 2 on
PF12 with `TriggerSource_TI2FP2`.

## Three things the schematic does not settle

1. **Y2 has no value.** The MCU crystal is drawn but unspecified, so HSE cannot
   be derived from the schematic. The `.ioc` keeps `RCC.HSE_VALUE=8000000` and
   the validated 550 MHz PLL tree lifted from the dosimeter project — which
   means **Y2 must be an 8 MHz part**, or `HSE_VALUE` and the PLL dividers both
   have to change. Decide this before ordering.

2. **No debug UART.** PD8/PD9 (USART3 on both older boards, and the ST-LINK VCP
   on the NUCLEOs) are unrouted here. `Bsp::Console` therefore has nowhere to go
   on this board: either accept a stub console and rely on SWO on PB3, or route
   a UART. 62 pins are spare, so there is room.

3. **PC2_C / PC3_C are the H7 dual-pad pins.** On LQFP144 only the `_C` pads are
   bonded, and they reach the digital logic through an analog switch. CubeMX
   does offer `GPIO` on both, but closing that switch is a SYSCFG step
   (`HAL_SYSCFG_AnalogSwitchConfig`) that plain `MX_GPIO_Init()` may not emit.
   Worth checking on first bring-up that `STEP_DIR` (PC2_C) actually drives and
   `SW_30MM_SELECTED` (PC3_C) actually reads.

## Before this builds

No STM32Cube FW_H7 package is installed, so CubeMX cannot generate the tree for
this `.ioc` yet — it needs the package downloaded first. Once it is:

1. Open `DegraderDosimeter.ioc`, let CubeMX fetch FW_H7, and generate.
2. Add `CMakeLists.txt`, `Board.cpp` and `BoardEthernet.cpp` modelled on
   `../NucleoF767ZI/` — change the chip define to `STM32H723xx` and the HAL
   paths to `STM32H7xx_HAL_Driver`.
3. Apply the same lwIP OS-mode overrides: the `USER CODE BEGIN 1` block in
   `../NucleoF767ZI/LWIP/Target/lwipopts.h` explains why they cannot live in the
   `.ioc`. The H723 is a different die, so check whether CubeMX will let this one
   set `WITH_RTOS` — the F767 restriction is specific to DIE451.
4. **Ethernet DMA reachability is the H7 version of the F767 problem.** The ETH
   DMA sits in domain D2 and cannot see DTCM or the D1 AXI SRAM, so the
   descriptors and the lwIP RX pool must go in D2 SRAM at `0x30000000`, with
   that region marked non-cacheable in the MPU. See the memory-region comments
   in `../NucleoF767ZI/STM32F767XX_FLASH.ld`.
