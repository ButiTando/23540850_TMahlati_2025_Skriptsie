# Host board — the firmware as a native Linux process

This is not a simulator. It is the same `ILT_OS`, the same lwIP, the same
application, built for x86-64 against the **FreeRTOS POSIX port** with a **TAP**
device standing in for the Ethernet MAC. It exists because the BSP split already
made it possible: nothing in `ILT_OS/Lib` or `ILT_OS/Applications` includes a
chip header, so "host" is just another board.

## What it does and does not test

| Exercised for real | Not exercised |
| --- | --- |
| Our threads and the RTOS wrappers (`Thread`, `Mutex`, `Queue`, …) | The STM32 HAL |
| `HttpServer`, the routes, the fs backend | `ethernetif.c`, the ETH driver, the PHY |
| `StreamServer` and its record pool | Linker layout, `.bss` placement, DMA reachability |
| Degrader/dosimeter logic, JSON formatting | MPU, caches, interrupt priorities |
| lwIP itself, end to end over a real socket | Real timing |

For the right-hand column you need hardware or Renode. The two are complements.

## Running it

```sh
cmake --preset Host && cmake --build --preset Host

# once per boot, needs root:
sudo BSP/Boards/Host/tap-setup.sh up

# the firmware itself runs unprivileged:
PRECONFIGURED_TAPIF=tap0 ./build/Host/FirmwareUpdate
```

Then, from another terminal:

```sh
curl http://192.168.7.2/api/status.json
curl "http://192.168.7.2/api/degrader/set.json?mask=42"
nc 192.168.7.2 5000            # the live record stream
xdg-open http://192.168.7.2/   # the control page
```

`sudo BSP/Boards/Host/tap-setup.sh down` removes the interface.

`PRECONFIGURED_TAPIF` is not optional. Without it lwIP's tapif tries to run
`ifconfig` itself, which would mean running the whole firmware as root.

The Host preset uses a static address (192.168.7.2) rather than the DHCP default
the boards use: there is no DHCP server on a point-to-point tap, so DHCP would
only add a 15-second timeout before the fallback.

## Debugging

Ordinary native tooling, which is most of the point:

```sh
gdb ./build/Host/FirmwareUpdate
valgrind --tool=helgrind ./build/Host/FirmwareUpdate      # thread races
cmake --preset Host -DCMAKE_C_FLAGS=-fsanitize=address \
                    -DCMAKE_CXX_FLAGS=-fsanitize=address
```

## Third-party code vendored for this board

| What | From | Licence |
| --- | --- | --- |
| `Middlewares/.../portable/ThirdParty/GCC/Posix/` | FreeRTOS-Kernel V10.4.6 | MIT |
| `netif/tapif.c`, `include/netif/tapif.h` | lwIP 2.2.0 contrib | BSD-3-Clause |

The POSIX port is V10.4.6 against our V10.2.0 kernel because the port did not
exist in the standalone kernel repo at 10.2.x. It needs only
`xTaskGetCurrentTaskHandle`, `xTaskGetIdleTaskHandle`, `xTaskIncrementTick`,
`vTaskSwitchContext`, `vTaskDelete` and `xTimerGetTimerDaemonTaskHandle`, all of
which 10.2.0 has.

## Local modifications, and why

Three changes were needed outside this directory. All are commented at the site.

1. **`cmsis_os2.c` — 64-bit handle truncation.** ST packs a "recursive mutex"
   flag into bit 0 of the mutex id and masks it with `(uint32_t)` casts. That is
   lossless only while a pointer is 32 bits; on x86-64 it truncates the handle
   and the next dereference segfaults. Changed to `uintptr_t`, which is
   identical on Cortex-M. **This is in a CubeMX-managed file — re-apply if it is
   regenerated.**

2. **lwIP POSIX compat headers removed from the global include path.**
   `src/include/compat/posix/**` exists so application code can call lwIP's
   sockets under POSIX names, which this firmware never does (`LWIP_SOCKET` is
   0). On the include path they make `<sys/socket.h>` and `<net/if.h>` resolve to
   lwIP shims instead of the system headers, which breaks any file wanting the
   real ones — `tapif.c` is exactly such a file.

3. **`StaticThread` gained a platform stack floor
   (`ILT_MIN_THREAD_STACK_BYTES`).** The POSIX port hands each task's stack
   straight to `pthread_attr_setstack()`, so anything under glibc's
   `PTHREAD_STACK_MIN` aborts at thread creation. Application threads keep their
   MCU-tuned sizes and are rounded up here.

`tapif.c` itself is unmodified; it is compiled with `-DLWIP_UNIX_LINUX` so it
takes the `/dev/net/tun` path rather than the BSD `/dev/tap0` one.
