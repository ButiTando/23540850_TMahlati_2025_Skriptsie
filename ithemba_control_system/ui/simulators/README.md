# Device simulators

Stand-ins for the two networked devices, so the control station can be
developed and tested without the rig.

```bash
python -m simulators.degrader     # 7-lens degrader   (TCP 1970 in, 1971 out)
python -m simulators.dosimeter    # 6-channel BLM     (TCP 1972 in, 1973 out)
```

Both default to imitating the firmware as closely as it is documented,
**including its awkward parts**, so that what works here works on the bench.
Add `-v` for one line per message, `-vv` for debug.

## Degrader

The real degrader plays two roles at once, and so does the simulator: it is a
TCP **server** on 1970 that receives commands, and a TCP **client** that dials
out to the control station on 1971 and reports its state every 500 ms whether
or not anything was asked of it.

| Wire element | Value |
|---|---|
| Command | 8-bit, ASCII decimal. Bit 0 = 2 mm … bit 6 = 30 mm, bit 7 = probe |
| Response | 16-bit, ASCII decimal, **no terminator**. Two bits per lens, bits 15-14 = process status |
| Lens status | 0 off, 1 on, 2 updating, 3 not changed |
| Process status | 0 busy, 1 awake, 2 error, **3 ready** (the resting state) |

Two firmware behaviours are reproduced by default because they cause real
trouble and code needs to cope with them:

- **Responses carry no terminator.** `--newline` adds one.
- **The "awake" status is never observable.** The firmware sets it and
  overwrites it in the same loop iteration, so a real degrader can never
  satisfy a check for status 1. `--hold-awake` keeps it visible for one send.

A command is latched when the sender closes the connection, matching the
firmware, which is why the control station opens a fresh socket per command.
`--latch-mode immediate` relaxes that for debugging.

Useful options: `--travel-ms` (how long one lens takes; moves are serialised as
on the hardware), `--initial-lenses 6mm,30mm`, `--fault-lens 3mm` (jams a lens
and reports an error), `--error-after N`.

### Cadence matters

A response whose value is 6553 or less can only be framed by the *gap* after
it, since nothing terminates it. The firmware's 500 ms cadence leaves ample
room, but driving `--interval` below ~0.25 s makes consecutive small values run
together. Small values occur while a command is executing, so use the default
cadence, or `--newline`, when testing fast.

## Dosimeter

Both devices now use the same shape: the dosimeter is a TCP **server** on 1972
for commands and a TCP **client** dialling the control station on 1973 with
telemetry. Every telemetry line ends in `\n`, so unlike the degrader this
stream is self-framing.

The command word is 4 bytes, big-endian, with an 8-bit opcode in the top byte
and 24 bits of payload — decoded on the board by
`tcp_server_take_command()` and `BLM_HandleCommand()`.

| Opcode | Command | Payload |
|---|---|---|
| `0x01` | SET_PERIOD | seconds, 1–3600 |
| `0x02` | SET_DATE | `(year << 16) \| (month << 8) \| day` |
| `0x03` | SET_TIME | `(hour << 16) \| (minute << 8) \| second` |
| `0x00` | legacy | date/time with no opcode — ambiguous, warned, guessed |

Telemetry is `YYYY-MM-DD HH:MM:SS,c1,…,c6\n`, one line per period.
**Only CSV is ever sent to the telemetry port** — acknowledgements go to the
simulator's log, because the control station parses everything arriving on that
port as CSV. The firmware does the same, acknowledging over its debug UART.

Counts are Poisson around `--rate`, shaped across the six detectors so the
display looks like a beam. `--beam-profile burst|ramp|off` makes it move;
`--dead-channel N` pins one channel to zero.

## Fault injection

Off by default. Every flag is seeded through `--seed`, so a failing run repeats.

| Flag | Effect |
|---|---|
| `--drop-rate P` | drop that fraction of outgoing messages |
| `--garbage-rate P` | replace that fraction with unparseable junk |
| `--stall SECONDS` | go silent after every 20 messages |
| `--split-writes` | dribble each message out in pieces |
| `--drop-connection N` | hang up after every N messages |

## Tests

```bash
python tests/test_framing.py        # unit: message recovery, no sockets
python tests/test_degrader_sim.py   # client <-> degrader simulator
python tests/test_dosimeter_sim.py  # client <-> dosimeter simulator
```

They use ports in the 219xx range, so they can run while the control station is
live on the real ones.

## Against the real rig

The devices expect the control station at `192.168.7.1`; the degrader is
`.104` and the BLM is DHCP-assigned (its firmware binds every interface, so it
answers on whatever address it is given).

```bash
python -m simulators.degrader  --control-ip 192.168.7.1
python -m simulators.dosimeter --control-ip 192.168.7.1
```
