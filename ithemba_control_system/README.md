# Ithemba Labs radiation test rig

Control-station software and the firmware for the two devices it talks to.

```
degrader/    STM32F746ZG firmware - 7-lens beam degrader
dosimeter/   STM32H723ZG firmware - 6-channel beam loss monitor (BLM)
ui/          Python control station (Tkinter) + device simulators
```

## The network

Everything is TCP. Each device runs a server for commands and dials the control
station back to report, so the station listens on two ports and connects out on
two others.

| Link | Direction | Port |
|---|---|---|
| Commands → degrader | station connects | 1970 |
| Responses ← degrader | degrader connects | 1971 |
| Commands → dosimeter | station connects | 1972 |
| Telemetry ← dosimeter | dosimeter connects | 1973 |

The control station is expected at **192.168.7.1**. The degrader is 192.168.7.104;
the dosimeter takes its address from DHCP and binds every interface.

### Degrader

Commands are one byte, sent as ASCII decimal: bit 0 is the 2 mm lens through to
bit 6 for 30 mm, and bit 7 is a probe flag. Responses are a 16-bit word, also
ASCII decimal, giving two bits per lens plus a process status in bits 15-14.

Responses carry **no terminator** and arrive every 500 ms. The client recovers
message boundaries from the value range, since a value never exceeds 65535 —
see `ResponseFramer` in `ui/radiation_systems/degrader.py`.

| Lens status | | Process status | |
|---|---|---|---|
| 0 | off | 0 | busy |
| 1 | in beam | 1 | awake |
| 2 | moving | 2 | error |
| 3 | not changed | 3 | ready (resting state) |

### Dosimeter

Commands are a 4-byte big-endian word: an 8-bit opcode then 24 bits of data.

| Opcode | Command | Payload |
|---|---|---|
| `0x01` | set period | seconds, 1–3600 |
| `0x02` | set date | `(year << 16) \| (month << 8) \| day` |
| `0x03` | set time | `(hour << 16) \| (minute << 8) \| second` |

Telemetry is one CSV line per period, newline-terminated:
`YYYY-MM-DD HH:MM:SS,c1,c2,c3,c4,c5,c6`. The same line also goes out over UART3
at 115200 baud as a debug view.

## Building the firmware

Both projects build in **STM32CubeIDE** with no setup beyond importing them.

1. *File → Import… → General → Existing Projects into Workspace*
2. *Select root directory*, browse to `degrader/` or `dosimeter/`, **Import**
3. *Project → Build Project* (or Ctrl+B)

Then flash with *Run → Debug* over ST-LINK. The degrader is a Nucleo-F746ZG and
the dosimeter a Nucleo-H723ZG; neither needs the other to be present to build.

The `Debug/` folder is deliberately not included — CubeIDE generates it on the
first build and discovers every source file under `Core/`, `LWIP/`, `Drivers/`
and `Middlewares/` on its own. Both projects have been verified to import and
build clean from exactly these folders.

<details>
<summary>Building from the command line instead</summary>

CubeIDE has to generate `Debug/` once before `make` has anything to run. After
one IDE build:

```bash
cd dosimeter/Debug && make -j4
```

If you add a source file and build this way, it must be listed in
`Debug/Core/Src/subdir.mk` **and** in `Debug/objects.list`, or it will compile
but never link. The IDE keeps both in step for you.
</details>

## Running the control station

Needs Python 3.10+ and Tk (`sudo apt install python3-tk` on Debian/Ubuntu).

```bash
cd ui
./setup.sh            # or .\setup.ps1 on Windows
source .venv/bin/activate
python ui.py
```

## Simulators

Both devices can be stood up in software, so the UI can be developed and tested
with no hardware present. They imitate the firmware's quirks by default.

```bash
python -m simulators.degrader
python -m simulators.dosimeter --beam-profile burst -v
```

Flags cover lens travel time, beam profiles, and deliberate misbehaviour
(dropped messages, split writes, forced disconnects) for testing how the station
copes. See `ui/simulators/README.md`.

## Tests

```bash
cd ui && ./tests/run_all.sh
```

Message framing is covered by unit tests; the rest run the real client against
the simulators over loopback, on ports in the 219xx range so they can run while
the station is live.
