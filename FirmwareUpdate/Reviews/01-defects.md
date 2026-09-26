# What went wrong — a review

Every defect found while restructuring the firmware, building the web UI and
bringing the board up on real hardware. Grouped by **who introduced it**,
because that is what decides whether the lesson is about the inherited design,
a third-party assumption, or the work done in this session.

Each entry gives the symptom first, because in almost every case the symptom
was nowhere near the cause.

| # | Defect | Origin | Severity |
|---|---|---|---|
| 1 | lwIP heap pointed at an STM32H7 address on an F7 | inherited | fatal |
| 2 | `.bss` linked into DTCM, which the Ethernet DMA cannot reach | inherited | fatal |
| 3 | ETH DMA descriptors were orphan sections | inherited | fatal |
| 4 | lwIP built bare-metal while FreeRTOS ran | inherited | fatal |
| 5 | Lens moves waited on a limit switch forever | inherited | serious |
| 6 | Lenses serviced in index order, ignoring the carousel | inherited | inefficiency |
| 7 | `cmsis_os2.c` truncates handles on 64-bit | third-party | fatal (host) |
| 8 | httpd sends by reference; our buffers are recycled | third-party assumption | **data corruption** |
| 9 | CubeMX cannot express this configuration | tooling limit | blocking |
| 10 | lwIP POSIX compat headers shadow system headers | third-party | build break |
| 11 | tapif reports `EINTR` as an error | third-party | noise |
| 12 | **Static constructors left interrupts masked** | **mine** | **fatal** |
| 13 | Query parameters never reached handlers | mine | feature dead |
| 14 | Seqlock could not detect a torn read | mine | latent |
| 15 | Status LED could never blink | mine | cosmetic |
| 16 | SMIL `<animate>` froze the plates | mine | visual |
| 17 | Carriage animation disabled while moving | mine | visual |
| 18 | Invented USER CODE section names | mine | regeneration loss |
| 19 | Lens checkboxes showed stale state | mine | UX |
| 20 | CSV download inert in the artifact viewer | mine | feature dead |
| 21 | Video played 12% slow | mine | cosmetic |
| 22 | **Receive path deadlocks when the pbuf pool empties** | third-party (CubeMX) | **fatal, silent** |
| 23 | **A linear selector axis modelled as a rotating carousel** | mine | **could damage the rig** |
| 24 | No homing: position assumed at power-on | mine | wrong after a reset mid-travel |

---

## Inherited defects

### 1. lwIP heap at an STM32H7 address

`lwipopts.h` set `LWIP_RAM_HEAP_POINTER 0x30020000`. That is D2 SRAM on an
STM32H7. On the F767, RAM ends at `0x20080000`, so the heap sat outside memory
and would have faulted on the first packet.

The `.ioc` carried the same value, so regenerating reproduced it. CubeMX's own
template says the parameter is meant for the H7 only — it should never have
appeared in an F7 project.

**Fix:** removed from the `.ioc`, and `#undef`'d in `lwipopts.h` so the heap
lives in `.bss` instead of at a fixed address.

### 2. `.bss` in DTCM

The linker script declared one 512 KB RAM region at `0x20000000`. The F767's
SRAM is contiguous but not uniform: `0x20000000` is DTCM, which is tied to the
core and **cannot be reached by the Ethernet DMA**. lwIP's heap and the
zero-copy RX pool would have been placed where the MAC cannot write.

**Fix:** split the regions (DTCM / SRAM1 / SRAM2) and link `.data`, `.bss`,
heap and stack into SRAM1. Verified from the ELF: RX pool at `0x20020794`,
lwIP heap at `0x2002c408`, DTCM 0 bytes used.

### 3. ETH DMA descriptors were orphan sections

`ethernetif.c` places `DMARxDscrTab`/`DMATxDscrTab` in `.RxDescripSection` and
`.TxDescripSection`. Neither section existed in the linker script, so the
linker placed them wherever it liked — potentially DTCM, where the DMA cannot
see them and the interface silently never receives a frame.

**Fix:** an explicit `.eth_dma` section in SRAM2. Verified: `DMARxDscrTab` at
`0x2007c000`, `DMATxDscrTab` at `0x2007c0a0` — the addresses ST hardcodes for
the IAR and Keil builds.

### 4. lwIP built bare-metal while FreeRTOS ran

`NO_SYS 1`, `WITH_RTOS 0`, and `MX_LWIP_Init()` was never called from `main()`
— the whole stack was dead code. `ethernetif.c` was the polled variant.

**Fix:** OS mode throughout; `ethernetif.c` converted to RX/TX semaphores and a
receive thread; bring-up moved into `ilt::net::NetworkStack`.

### 5. Lens moves waited forever

`degrader_utils.c` polled a limit switch with no timeout. A jammed carousel or
a failed switch hung the superloop **with a motor still energised**.

**Fix:** a 30-second per-move deadline; on expiry both motors stop and the
mechanism reports `Fault`.

### 6. Lenses serviced in index order

`reconcile()` walked lenses 0..6 regardless of where the carousel was. The
stepper only turns one way, so asking for 2 mm and 30 mm while parked at 30 mm
drove most of the way round, then most of the way round again.

**Fix:** pick the pending lens with the smallest forward distance each time —
one sweep instead of several laps. Measured on the rig, parked at 12 mm then
asked for 30 mm + 2 mm: **400 steps instead of 1,600**.

---

## Third-party defects and assumptions

### 7. `cmsis_os2.c` truncates handles on 64-bit

ST's CMSIS-RTOS v2 wrapper packs a "recursive mutex" flag into bit 0 of the
mutex handle and masks it with `(uint32_t)` casts. Lossless while a pointer is
32 bits; on x86-64 it destroys the pointer.

**Symptom:** the host build segfaulted on the first run, in
`xQueueSemaphoreTake` with a handle whose top 16 bits were gone.

**Fix:** `uintptr_t` casts — identical on Cortex-M. **This is a CubeMX-managed
file; re-apply if it is regenerated.**

### 8. httpd sends by reference, our buffers are recycled

This is the most serious defect found, and the one worth reading twice.

lwIP's httpd defaults `HTTP_IS_HDR_VOLATILE` and `HTTP_IS_DATA_VOLATILE` to
"reference, do not copy". That is correct for its own fsdata: a `const` array
in flash that never changes and never goes away.

**Neither assumption holds here.** Dynamic routes render into a pooled RAM
buffer that `fs_close()` hands straight back for the next request. Sending it
by reference left lwIP transmitting out of a buffer already recycled — a
use-after-free whose visible form would be **one client receiving another
client's response**.

It also exhausted memory: a referenced write allocates a `PBUF_ROM` from
`MEMP_PBUF` and holds it until ACKed. That pool is 16 entries.

**Symptom on hardware:** httpd accepted TCP connections and never answered.
`tcp_write` returned `ERR_MEM` on a 17-byte header despite `snd_buf=5840` and
`snd_queuelen=0`. A pool census showed `MEMP_PBUF` **0 free of 16** while every
other pool had capacity.

**Fix:** force `TCP_WRITE_FLAG_COPY` for both headers and data. The bytes go
into a `PBUF_RAM` from the lwIP heap, and the render buffer is reclaimable
immediately.

Note the stream on port 5000 worked from the first flash, because it always
copied. That contrast is what isolated the fault to httpd rather than the stack.

### 9. CubeMX cannot express this configuration

`WITH_RTOS=1` is gated on `!NO_SYS & !(DIE451 & (FreeRTOS_API = 1))`.
`DIE451` is the STM32F76x/F77x die — **confirmed on the hardware, which reports
Device ID 0x451** — and `FreeRTOS_API = 1` is CMSIS-RTOS v2. So on this exact
part with v2 the option does not exist, and CubeMX always emits `NO_SYS 1`.

Verified by regenerating and diffing, not assumed: setting the keys by hand had
no effect, the resolver discarded them.

**Consequence:** `lwipopts.h` is CubeMX's own output plus a `USER CODE BEGIN 1`
block carrying the OS-mode settings — regeneration is now a no-op there.
`ethernetif.c` **cannot** be protected the same way, because the RTOS
conversion replaces generated function bodies. A checklist is in that file's
header; a revert compiles cleanly and then receives no packets.

### 10. lwIP POSIX compat headers shadow system headers

`src/include/compat/posix/**` exists so application code can call lwIP's
sockets under POSIX names. I had put those directories on the global include
path defensively. They make `<sys/socket.h>` and `<net/if.h>` resolve to lwIP
shims instead of the real headers, which broke the host TAP driver.

**Fix:** removed from the include path — nothing uses them (`LWIP_SOCKET` is 0).
This was also a latent hazard on the ARM build.

### 11. tapif reports `EINTR` as an error

72,000 log lines in one run. The FreeRTOS POSIX port drives scheduling with
signals, so `select()` is interrupted constantly.

Filtering on `EINTR` did not work either: the port's handlers do not preserve
`errno`, and it reads back as 0 as often as 4. Under this port errno simply
cannot distinguish a scheduling interruption from a real failure.

**Fix:** report the first occurrence with the raw errno, then stay quiet.

---

## Defects introduced in this session

### 12. Static constructors left interrupts masked — the board booted dead

The worst of mine, and the one whose symptom was furthest from its cause.

C++ static constructors run before `main()`. `rig::Degrader` holds an
`ilt::Queue` and `rig::Dosimeter` an `ilt::Mutex`, so both created FreeRTOS
objects during `__libc_init_array`. Creating any FreeRTOS object takes a
critical section, and `vPortExitCritical()` only restores interrupts when
`uxCriticalNesting` reaches zero — but the Cortex-M port initialises that
counter to `0xaaaaaaaa` and only zeroes it in `xPortStartScheduler()`.

So every critical section before the scheduler masks interrupts **permanently**.

**Symptom:** the board was silent. No console output, no fault set. `uwTick`
frozen at 1. TIM7 was configured correctly and counting, its update flag
pending, its NVIC line enabled, its vector pointing at a real handler — and the
interrupt never fired. The first `HAL_Delay()` in USB init spun forever.

Measured on the target: `basepri = 0x50`, `uxCriticalNesting = 0xaaaaaaaa`.

**Fix:** `__set_BASEPRI(0)` at the top of `main()`, before the first HAL call,
with the reasoning written out at the site.

**Why I missed it:** I considered this exact risk when writing `Mutex` and
concluded it was safe because FreeRTOS permits creating a statically allocated
object before the scheduler starts. That is true of the *object*; I missed the
critical-section side effect. CubeMX projects avoid it by doing all peripheral
init before touching FreeRTOS — static constructors get in ahead of that.

The host build could not have caught it: the POSIX port has no BASEPRI.

### 13. Query parameters never reached handlers

`?mask=42` was rejected identically to omitting it. lwIP's httpd
**NUL-terminates the URI at `?` before calling `fs_open`**, so the handlers
never saw one. My parser was fine; it was never given anything.

**Fix:** `LWIP_HTTPD_CGI_SSI`, and re-render in `httpd_cgi_handler()` — called
after `fs_open` but before httpd reads `file->len`, the one point where the
parameters and the file exist together. No patch to httpd.c. My hand-rolled
parser was replaced by an `HttpQuery` object using lwIP's own parsing.

### 14. Seqlock could not detect a torn read

`Dosimeter::latest()` used a sequence counter the writer only bumped **after**
writing. A reader copying mid-update saw the same value before and after and
accepted a mixed sample.

**Fix:** an `ilt::Mutex`. A 32-byte copy a few times a second does not justify
a hand-rolled lock-free scheme.

### 15. Status LED could never blink

`ledSet(false)` followed by `ledToggle()` lands on "on" every iteration.

### 16. SMIL `<animate>` froze the plates

`<animate fill="freeze">` snapshots the value on its first run and then
overrides every later attribute update, so plates stayed where they first
appeared — every lens rendered parked despite correct data. Replaced with a CSS
transition.

### 17. Carriage animation disabled while moving

`transition: turning ? 'none' : ...` — backwards. The transition was off
exactly while the stepper turned, so the carriage teleported a whole station
once per second. It also made the recording 9 fps, because with no animation
running the page barely repainted.

**Fix:** interpolate linearly across the poll interval while turning; poll at
250 ms during motion instead of 1 s. Recording went to 32.7 fps.

### 18. Invented USER CODE section names

I used `MACADDRESS_FETCH`, `RTOS_STATE`, `RTOS_INIT`, `LOW_LEVEL_OUTPUT` in
`ethernetif.c`. CubeMX only preserves sections it emits, so a regeneration test
dropped all of them — including the MAC-from-unique-ID call. The real section
is `MACADDRESS`; that one is fixed and verified surviving. The others are
structural (see #9).

### 19. Lens checkboxes showed stale state

The first page wrote the lens mask but never read it back, so the controls
showed stale state after a reload or to a second viewer. The React UI now
tracks the board's desired mask and marks any lens where request and mechanism
disagree.

### 20. CSV download inert in the artifact viewer

Plain download links do nothing in a published artifact. The headline CSV
feature would have silently failed. Routed through the `downloads` capability
with the blob-anchor path kept as a fallback.

### 21. Video played 12% slow

A 0.02 s minimum frame duration inflated gaps whenever frames arrived faster
than 50 fps, stretching 38.9 s into 43.5 s.

---

## Process notes

Two things cost real time and are worth remembering.

**Orphaned processes nearly caused a misdiagnosis.** A test script used a bare
`wait`, which also waits on the firmware — so a timeout left five firmware
processes running, all writing to the same log with *older binaries*. A fix
that had worked appeared not to, because the log still showed 18,000 stale
messages. Always `wait` on specific PIDs.

**`pkill -f <pattern>` matches its own command line** and kills the shell
running it. Cost several confusing exit-code-1 failures. Match on process name.

---

## What is still untested

- Every degrader and dosimeter **hardware** path. The Nucleo has neither, and
  the DegraderDosimeter board has no CubeMX tree yet. The state machine has
  only ever run against the host simulation.
- `steps_per_station = 200` is a **placeholder**. It is a mechanical constant
  the schematic does not record, and every position estimate scales with it.
- The `.ioc` for the DegraderDosimeter board has not been opened in CubeMX —
  no STM32Cube FW_H7 package is installed, so it has never been generated from.
- HSE on the DegraderDosimeter board: crystal Y2 has no value in the schematic.
  The `.ioc` assumes 8 MHz to keep the validated PLL tree.

---

## 22. The receive path deadlocks when the pbuf pool empties

**Symptom.** The board serves for one to three minutes, then vanishes. No
console output, no fault. The MCU is fine: `uwTick` keeps advancing. The PHY
still reports link up, so the firmware never prints `net: link down`; the host's
`ethtool` also says link up at 100 Mb/s full duplex. The host ARPs into silence
and its neighbour entry sits at `INCOMPLETE`. Only a reset recovers it.

**Cause.** In CubeMX's `ethernetif.c`, the receive path has a state it cannot
leave. `HAL_ETH_RxAllocateCallback` sets `RxAllocStatus = RX_ALLOC_ERROR` when
the zero-copy pool is exhausted, and `low_level_input` then refuses to call
`HAL_ETH_ReadData` at all. With no buffer to fill, the DMA stops raising receive
interrupts — and the receive interrupt is the *only* thing that posts
`RxPktSemaphore`, which `ethernetif_input` waits on with `osWaitForever`. The
one event that could restart reception is the one that can no longer happen.

`pbuf_free_custom` clears the flag when a buffer comes back, and its own comment
says it should "signal the ethernetif_input task to call
HAL_ETH_GetRxDataBuffer to rebuild the Rx descriptors" — but the generated code
never signals anything. The comment describes a step that was never written.

The pool is 12 buffers against 4 descriptors, so it only empties under sustained
traffic; a long-lived stream connection plus HTTP polling is enough.

**Fix.** Three changes, marked `MODIFIED (not CubeMX)` in the file:

1. `pbuf_free_custom` releases `RxPktSemaphore`, doing what its comment says.
2. `ethernetif_input` waits with a 1 s timeout instead of `osWaitForever`, and
   drains regardless of how it woke. Any path that stops the interrupts also
   stops the thread that would restart them, so the thread must not depend on
   them; a timeout costs one wasted pass a second and removes the whole class of
   deadlock.
3. After draining, `ETH_DMASR.RBUS` is cleared and a receive poll demand is
   written to `DMARPDR`. Returning descriptors is not enough on its own — a
   suspended receive DMA only restarts when the flag is cleared and it is told
   to re-poll.

`/api/status.json` now reports `rx.frames` and `rx.dma_resumes`, so a recurrence
is a number that stops climbing rather than a silent network.

**One measurement does not fit.** When the fault was caught live, `ETH_DMASR`
read `RPS=3` (waiting for a packet) with `RBUS=0` — a receive DMA that is armed
and idle, not one starved of descriptors. That is the state expected if no
frames were reaching the MAC at all, which would point at the RMII/PHY layer
rather than at this deadlock. The reading was taken minutes after the failure,
so the DMA may have settled, but it is not proof either way. The deadlock above
is real, provable from the source, and worth fixing regardless; whether it was
*this* failure is confirmed only by the board now running past the point where
it used to die.

**This file is CubeMX-managed.** Regenerating drops all three changes and the
board goes back to dying after a couple of minutes. See defect 18.

---

## 23. A linear selector axis modelled as a rotating carousel

**Symptom.** The selector ran to one end of the rail and reappeared at the other,
repeatedly, for moves that should have been a single step.

**Cause.** `ILT_TESTRIG`'s degrader was written around a carousel: stations on a
ring, distance modulo seven, travel past the last station wrapping to the first.
Nothing in the rig works that way. The original firmware settles it —
`degrader_utils.c` picks direction with a plain comparison on a line:

```c
stepper_direction = (dc_motor_position < lenses[i]->position) ? forwards : backwards;
```

lens positions are `1..7` in `main.c`, and initialisation homes by driving
*backwards* until the 2 mm select switch closes: "send the DC motor to the home
position (2mm lens)". That is a linear axis with an end stop. There is no
modular arithmetic anywhere in the original.

The carousel was my invention in an earlier session, asserted as fact in the
BSP contract, the application headers and the drawing, and never checked against
the firmware it was replacing.

**Why it was worse than it looked.** The visible bug was that rotation was
forward-only, so a station one step behind cost a full lap. Fixing *that* first
— bidirectional shortest-path, still modulo seven — made it dangerous rather
than merely wasteful: station 6 to station 0 became "one step forward across the
seam", 832 ms, which on the real mechanism is the carriage driving into the end
stop at full rate. A wrong model produced a plausible optimisation that a right
model forbids.

**Fix.** Distance is `|target - current|` and direction is its sign. Position is
clamped to the axis, never wrapped. Both rig models grew end stops, so the model
now *shows* the collision instead of hiding it. The drawing has a home stop and
an end stop in place of the loop-closure arrows, and the word "carousel" is gone
from the BSP contract, the headers and the UI, because it was asserting a
mechanism that does not exist.

**Lesson.** The original firmware was in the repository the whole time. One
`grep` for the direction logic would have settled the mechanism before any of
this was built on top of it.

---

## 24. No homing: position assumed at power-on

**Symptom.** None visible in the simulation, which always starts at station 0.
On hardware, every position would be wrong after any reset that left the
carriage mid-travel, and the first commanded move would drive from an imaginary
starting point.

**Cause.** `Degrader::run()` initialised the BSP and went straight to serving
requests. Step counting is dead reckoning from an assumed origin, and nothing
had ever measured that origin.

**Fix.** `Degrader::home()`, run before any request is accepted, in the two
stages the original uses: read every SELECT switch first and adopt the station
if one is already closed; otherwise drive toward the home end until station 0's
switch closes. Failure leaves the mechanism stopped and the status in Fault
rather than moving on a guess — where the original spins forever in
`while(read_Switch(...) != GPIO_PIN_SET) __NOP();`, which on a broken rig hangs
the board silently.

Two related corrections came with it. A SELECT switch closing now re-datums the
position, as the original does when it assigns the lens's own position on
completion; the correction that takes is reported as `drift_steps` so that
snapping cannot hide a motor losing steps. And a lens whose SELECT switch is
already closed no longer starts the stepper at all, matching the original's
check of position before direction.

---

## 25. Two threads shared the TX descriptor bookkeeping — the board went deaf

**Symptom.** Under sustained load — four concurrent page loads, an SSE
subscriber and fast JSON polling at once — the board stopped answering after
five to nine rounds. Not just HTTP: ARP went `INCOMPLETE`, ping lost 100%. The
serial console kept printing, so the firmware was alive, and its own counters
told a contradictory story: `tcp x` (transmit) climbing, `tcp r` (receive)
frozen, and `eth: rx frames` still climbing. It was receiving frames and
answering none of them. This is the "board keeps failing when I connect" that
had been reported repeatedly and never pinned down.

**Diagnosis.** Worth recording in full, because every step ruled out a
plausible wrong answer.

1. The profiler, echoed to the UART, showed `tcpip` blocked at ~0% CPU. Not
   spinning; waiting.
2. GDB attached *without a reset* (`ST-LINK_gdbserver --attach`), so the
   failed state survived. Walking FreeRTOS's task lists found exactly one task
   blocked with no timeout: `Tmr Svc`, which is normal. `tcpip_thread` was on
   the delayed list — idle, waiting on its mailbox with a timer. It was not
   deadlocked on a mutex, which was the first hypothesis.
3. The tcpip mailbox held 0 messages. So inbound frames were being counted by
   the driver and then discarded before reaching lwIP.
4. `MEMP_TCPIP_MSG_INPKT` showed `max 8, err 748`: that pool *had* been
   exhausted during the burst and dropped 748 packets. But it was 1/8 now, and
   the board was still deaf — damage from the burst, not the standing fault.
5. RX was demonstrably working, which left TX. `DMASR` read `TPS = 6`
   (transmit DMA **suspended**) with `TBUS = 1`. `TxPktSemaphore = 0`. So every
   `low_level_output()` waited its timeout and dropped the frame. `tcp x`
   climbing only meant lwIP *believed* it had transmitted.
6. A false alarm here cost twenty minutes: dumping the TX descriptor ring with
   a 32-byte stride showed what looked like memory corruption. This HAL's
   `ETH_DMADescTypeDef` is **40 bytes** (enhanced descriptors plus two backup
   words). Re-read at the right stride, all four descriptors were perfectly
   formed and every `OWN` bit was clear — the ring was completely free.
7. With a free ring and the DMA merely suspended, `HAL_ETH_Transmit_IT()`
   should have succeeded and issued the poll demand that restarts it. A
   breakpoint on a live transmit showed it returning `HAL_ERROR` with
   `desc[CurTxDesc].OWN = 0`. `ETH_Prepare_Tx_Descriptors()` treats a
   descriptor as busy if `OWN` is set **or** `PacketAddress[i] != NULL`, and
   the handle read `PacketAddress = {0, pbuf, pbuf, 0}` with `BuffersInUse = 0`.
   `HAL_ETH_ReleaseTxPacket()` only clears those slots while `BuffersInUse`
   is non-zero. Slots 1 and 2 could never be cleared. Permanent BUSY.

**Cause.** `HAL_ETH_ReleaseTxPacket()` was being called from *two* threads:
`ethernetif_input` (priority 48) and `low_level_output` (tcpip_thread,
priority 40). It and `HAL_ETH_Transmit_IT()` both read-modify-write the same
`TxDescList` fields (`BuffersInUse`, `releaseIndex`, `PacketAddress[]`) with no
lock. The RX thread, being higher priority, could preempt a `Transmit_IT`
half-way through; when it did, an increment of `BuffersInUse` was lost, the
slots that increment covered were orphaned, and the ring wedged the next time
`CurTxDesc` reached one of them.

**Fix.** Reclamation happens only on tcpip_thread, in `low_level_output()`,
before every transmit rather than only when the ring is full. Also there: if
`TBUS` is set, clear it and issue a poll demand. `Transmit_IT` only does that
if it happens to observe the flag, and the DMA raises it a few cycles after
deciding to suspend, so a call landing in that window would otherwise leave the
DMA parked with a ready descriptor in front of it.

**Verification.** The same load that killed the board at round 5 and round 9
ran 25 rounds clean. Peak RX-pool occupancy fell from 12/12 to 4/12, with zero
exhaustion — with TX no longer stalling, connections complete and stop holding
receive buffers waiting for acknowledgements that never come.

**Lesson.** Every counter this firmware reported was on the *network* side,
and a board that cannot transmit has nothing to report there. The `eth:` and
`prof:` console lines added during this investigation exist so the next stall
of this kind is a five-minute read of the UART, not an afternoon with a
debugger. See `Documentation/NetworkDiagnostics.md`.

---

## 26. `snd_queuelen` underflowed on zero-copy sends — pages truncated at 8 s

**Symptom.** After the web UI grew from 65 KB to 178 KB (Recharts), page loads
truncated at a random offset about half the time, always after exactly 8
seconds. Small JSON responses were unaffected.

**Cause.** The website was sent zero-copy from flash (`PBUF_ROM`) as an
economy. A transfer large enough to exercise the segment split/realloc paths in
`tcp_out.c` corrupts the accounting: `pcb->snd_queuelen` is a `u16_t` and was
read as **65412** (= 65536 − 124) on a connection that was otherwise perfectly
healthy — everything acknowledged, nothing outstanding, send buffer free,
window wide open. Since `tcp_write()` refuses while `snd_queuelen >=
TCP_SND_QUEUELEN` (16), every write then failed `ERR_MEM`, httpd made no
progress, and `http_poll()` closed the connection after
`HTTPD_MAX_RETRIES × HTTPD_POLL_INTERVAL` = 8 s. Instrumenting `http_poll()`
is what caught it (`ILT_HTTPD_DIAG` in `httpd.c`, now compiled out).

**Fix.** `HTTP_IS_DATA_VOLATILE` returns `TCP_WRITE_FLAG_COPY` for everything.
Disabling `TCP_OVERSIZE` did not help; only copying did. The heap objection
that produced the original zero-copy decision no longer applies: in-flight data
per connection is bounded by `TCP_SND_BUF` (5840 B), not by file size, and the
heap is 48 KB now rather than 16. Measured under four concurrent page loads:
heap 60 B used, no pool exhausted; 15/15 then 12/12 complete transfers,
`tcp.memerr` from 14 to 0. The full reasoning is in `lwipopts.h` so nobody
optimises it back.

---

## 27. Two receive-side pools were invisible to every diagnostic

**Symptom.** None on their own — this is why the burst in defect 25 did as
much damage as it did before the standing fault took over.

**Cause.** The console reported lwIP's generic pbuf pool (`MEMP_PBUF_POOL`),
which sat at 0/12 throughout. Two other pools were saturating unseen: the
driver's zero-copy `RX_POOL` (12 buffers, a custom `LWIP_MEMPOOL` that lwIP's
statistics never mention) hit 12/12 with 472 failed allocations, and
`MEMP_TCPIP_MSG_INPKT` (8 entries) hit 8/8 with 748 failed allocations — each
one an inbound packet silently dropped between the driver and lwIP.

**Fix.** Both are now on the console heartbeat (`eth:` line: `rxpool u/12 max
ERR alloc=` plus the DMA receive state), printed on change and every 5 s. Not
resized: with the TX stall fixed neither pool exceeded 4/12 under the same
load, and a larger pool would have masked defect 25 rather than fixed it.

---

## 28. The cleanup after every screenshot was killing the user's browser

**Symptom.** Tabs vanishing on the user's machine during the session.

**Cause.** Headless Chrome instances launched for screenshots were cleaned up
with `for p in $(pgrep -x chrome); do kill -9 $p; done`. `pgrep -x` matches on
process *name*, and the user's own browser is also named `chrome`. Verified:
the user's browser PID was in that list.

**Fix.** Match on something unique to the launched instance — the scratchpad
`--user-data-dir` — and filter on `comm == "chrome"` so the pattern can never
match the shell running it:

    ps -eo pid=,comm=,args= | awk -v p="user-data-dir=/tmp/claude" \
        '$2=="chrome" && index($0,p)>0 {print $1}'

Related, and hit four times in one session: `pkill -f` / `pgrep -f` with a
pattern that also appears in the invoking command line matches the invoking
shell and kills it (exit 144). The `[x]` bracket trick only helps when the
literal string is absent from the command; when it is present — a path in the
same command, say — it does nothing. Capture the PID at launch, or select by
listening port (`ss -ltnp`), and never by name.

---

## 29. Recharts charts rendered blank, then one grew until it ate the panel

**Symptom.** Dosimeter trend chart empty after the move to Recharts; CPU chart
briefly expanding on every observation until nothing else fitted.

**Cause.** Two different things. `ResponsiveContainer` measures its parent, and
dropped straight into a flex container it has no flex-basis and no intrinsic
size, so it collapsed to zero. Separately, the hand-rolled chart's
`ResizeObserver` measured a box that held both the SVG *and* the legend, so
each measurement fed the legend's height back into the SVG's height — a growth
loop.

**Fix.** Every chart sits in an absolutely-positioned `.chart-box` (`inset: 0`)
inside a positioned parent, which needs no resolvable parent height at all —
`height: 100%` was the first attempt and only worked where the parent had an
explicit height. The measured box, when there is one, holds only the SVG and
takes its size from the flex layout, not from its contents.

---

## 30. One undefined field blanked the entire page

**Symptom.** Opening the Platform tab rendered an empty page — including the
beam readouts on the Rig tab, which had nothing to do with it.

**Cause.** The firmware emits `"pos"`; the panel read `state.position`;
`undefined.toLocaleString()` threw during render. React unmounts the whole
tree when a render throws, and there was no error boundary.

**Fix.** The field name, and an `ErrorBoundary` around every card. A panel that
fails now says so inside its own frame and offers to retry; its neighbours keep
updating. On a page someone is watching mid-beam, a blank screen is the worst
possible failure because it looks exactly like the board having died.
