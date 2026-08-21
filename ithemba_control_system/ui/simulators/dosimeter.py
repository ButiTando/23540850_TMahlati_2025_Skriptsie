"""Dosimeter (BLM) simulator: TCP server on 1972 for commands, TCP client to
the control station on 1973 streaming CSV telemetry - matching the firmware in
Firmware/BLM/BLM_Code. Unlike the degrader, every line is newline-terminated,
so the stream is self-framing.

    python -m simulators.dosimeter
    python -m simulators.dosimeter --beam-profile burst -v
"""

import logging
import math
import random
import socket
import struct
import threading
import time
from datetime import datetime, timedelta

from ._common import (FaultInjector, add_fault_args, base_parser, close_quietly,
                      configure_logging, enable_keepalive, make_tcp_listener,
                      run_until_interrupt)

COMMAND_PORT = 1972          # we listen here for commands
TELEMETRY_PORT = 1973        # we dial out to here with telemetry
RECONNECT_DELAY = 2.0        # seconds before retrying the outbound link
CHANNELS = 6

# Opcodes, matching radiation_systems/dosimeter.py.
CMD_SET_PERIOD = 0x01
CMD_SET_DATE = 0x02
CMD_SET_TIME = 0x03
CMD_NAMES = {CMD_SET_PERIOD: "SET_PERIOD", CMD_SET_DATE: "SET_DATE",
             CMD_SET_TIME: "SET_TIME"}


def poisson(rng, mean):
    """Draw a Poisson count without pulling in numpy."""
    if mean <= 0:
        return 0
    if mean < 30:
        # Knuth: multiply uniforms until the product drops below e^-mean.
        limit = math.exp(-mean)
        count, product = 0, rng.random()
        while product > limit:
            count += 1
            product *= rng.random()
        return count
    # Normal approximation: indistinguishable this far out, and no loop.
    return max(0, int(round(rng.gauss(mean, math.sqrt(mean)))))


class DosimeterSim:
    def __init__(self, args):
        self.args = args
        self.log = logging.getLogger("dosimeter-sim")
        self.rng = random.Random(args.seed)
        self.faults = FaultInjector(args, self.rng)

        self.period = max(1, args.period)
        self._clock_offset = timedelta(0)
        self._sent = 0

        # Gaussian across the six detectors, so it reads as a beam.
        centre = (CHANNELS - 1) / 2.0
        self.channel_gain = [math.exp(-((i - centre) ** 2) / 3.0) for i in range(CHANNELS)]

        self.state_lock = threading.Lock()
        self.stop_event = threading.Event()
        self.listener = None
        self.telemetry_sock = None
        self.threads = []

    # ---------------- lifecycle ----------------

    def start(self):
        self.listener = make_tcp_listener(self.args.listen_ip, self.args.command_port)
        self.listener.settimeout(0.5)
        for name, target in (("cmd-rx", self._command_loop),
                             ("telemetry", self._telemetry_loop)):
            thread = threading.Thread(target=target, name=name, daemon=True)
            thread.start()
            self.threads.append(thread)

    def stop(self):
        self.stop_event.set()
        close_quietly(self.listener)
        close_quietly(self.telemetry_sock)
        for thread in self.threads:
            thread.join(timeout=2.0)
            if thread.is_alive():
                self.log.warning("thread %s did not stop", thread.name)

    # ---------------- commands ----------------

    def _command_loop(self):
        while not self.stop_event.is_set():
            try:
                conn, addr = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                self._serve_command(conn, addr)
            except OSError as e:
                self.log.debug("command connection %s: %s", addr, e)
            finally:
                close_quietly(conn)

    def _serve_command(self, conn, addr):
        """Read one command connection; the station opens a fresh one each time."""
        conn.settimeout(2.0)
        chunks = []
        while not self.stop_event.is_set():
            try:
                data = conn.recv(64)
            except socket.timeout:
                break
            if not data:
                break
            chunks.append(data)

        data = b"".join(chunks)
        for offset in range(0, len(data) - 3, 4):
            self._handle_word(data[offset:offset + 4], addr)
        if len(data) % 4:
            self.log.warning("%d trailing bytes ignored from %s",
                             len(data) % 4, addr[0])

    def _handle_word(self, word, addr):
        value = struct.unpack(">I", word)[0]
        opcode = (value >> 24) & 0xFF
        payload = value & 0xFFFFFF

        if opcode == CMD_SET_PERIOD:
            new_period = max(1, min(payload, 3600))
            with self.state_lock:
                old, self.period = self.period, new_period
            self._reply(addr, f"ACK SET_PERIOD {old} -> {new_period}s")
        elif opcode == CMD_SET_DATE:
            self._set_date(payload >> 16, (payload >> 8) & 0xFF, payload & 0xFF, addr)
        elif opcode == CMD_SET_TIME:
            self._set_time(payload >> 16, (payload >> 8) & 0xFF, payload & 0xFF, addr)
        elif opcode == 0x00:
            self._handle_legacy(payload, addr)
        else:
            self._reply(addr, f"WARN unknown_cmd 0x{opcode:02X} data=0x{payload:06X}")

    def _handle_legacy(self, payload, addr):
        """Decode a pre-opcode date/time word.

        Older builds left the opcode zero, so the two are indistinguishable and
        the guess below is wrong for the years 2000-2023. Warn every time.
        """
        first = (payload >> 16) & 0xFF
        rest = ((payload >> 8) & 0xFF, payload & 0xFF)
        if first > 23:
            self.log.warning("ambiguous 0x00 word 0x%06X read as a DATE "
                             "(sender should use opcode 0x02)", payload)
            self._set_date(first, rest[0], rest[1], addr)
        else:
            self.log.warning("ambiguous 0x00 word 0x%06X read as a TIME "
                             "(sender should use opcode 0x03; a year of "
                             "2000-2023 would be misread here)", payload)
            self._set_time(first, rest[0], rest[1], addr)

    def _set_date(self, year, month, day, addr):
        absolute = 2000 + year if year < 100 else 1900 + year
        month = max(1, min(month, 12))
        # Clamp rather than raise; the old version killed its receive thread.
        if month == 2:
            last = 29 if (absolute % 4 == 0 and (absolute % 100 or absolute % 400 == 0)) else 28
        else:
            last = 30 if month in (4, 6, 9, 11) else 31
        day = max(1, min(day, last))
        current = self.now_sim()
        self._set_clock(current.replace(year=absolute, month=month, day=day))
        self._reply(addr, f"ACK SET_DATE {absolute:04d}-{month:02d}-{day:02d}")

    def _set_time(self, hour, minute, second, addr):
        hour, minute, second = min(hour, 23), min(minute, 59), min(second, 59)
        current = self.now_sim()
        self._set_clock(current.replace(hour=hour, minute=minute,
                                        second=second, microsecond=0))
        self._reply(addr, f"ACK SET_TIME {hour:02d}:{minute:02d}:{second:02d}")

    def _reply(self, addr, text):
        """Record the outcome of a command.

        Never sent to the telemetry port: the station parses everything there
        as CSV, so an ACK would read as a parse error. The firmware likewise
        acknowledges over its debug UART.
        """
        self.log.info("%s", text)

    # ---------------- simulated clock ----------------

    def now_sim(self):
        # Local wall-clock, not UTC: the log filenames are local too.
        return datetime.now() + self._clock_offset

    def _set_clock(self, target):
        self._clock_offset = target - datetime.now()

    # ---------------- telemetry ----------------

    def _beam_scale(self):
        profile = self.args.beam_profile
        if profile == "off":
            return 0.0
        if profile == "steady":
            return 1.0
        elapsed = self._sent * self.period
        if profile == "burst":
            phase = (elapsed % self.args.burst_period) / self.args.burst_period
            return 1.0 if phase < self.args.burst_duty else 0.02
        if profile == "ramp":
            return 0.05 + 0.95 * ((elapsed % self.args.burst_period) / self.args.burst_period)
        return 1.0

    def _telemetry_loop(self):
        while not self.stop_event.is_set():
            if self.telemetry_sock is None and not self._dial():
                if self.stop_event.wait(RECONNECT_DELAY):
                    break
                continue
            with self.state_lock:
                period = self.period
            if not self._send_telemetry():
                close_quietly(self.telemetry_sock)
                self.telemetry_sock = None
                continue
            if self.stop_event.wait(period):
                break

    def _dial(self):
        target = (self.args.control_ip, self.args.telemetry_port)
        try:
            sock = socket.create_connection(target, timeout=3.0)
        except OSError as e:
            self.log.debug("control station %s:%d unreachable: %s", *target, e)
            return False
        enable_keepalive(sock)
        self.telemetry_sock = sock
        self.log.info("connected to control station %s:%d", *target)
        return True

    def _send_telemetry(self):
        with self.state_lock:
            period = self.period
        scale = self._beam_scale()
        counts = []
        for index in range(CHANNELS):
            if index + 1 == self.args.dead_channel:
                counts.append(0)
                continue
            mean = self.args.rate * self.channel_gain[index] * scale * period
            counts.append(poisson(self.rng, mean))

        stamp = self.now_sim().strftime("%Y-%m-%d %H:%M:%S")
        line = f"{stamp}," + ",".join(str(c) for c in counts) + "\n"
        self._sent += 1

        if self.faults.should_drop():
            return True
        line = self.faults.corrupt(line.rstrip("\n"))
        if not line.endswith("\n"):
            line += "\n"

        payload = line.encode("ascii", "ignore")
        try:
            for chunk in self.faults.chunks(payload):
                self.telemetry_sock.sendall(chunk)
                if len(payload) > len(chunk):
                    time.sleep(0.003)
        except OSError as e:
            self.log.info("telemetry send failed, will redial: %s", e)
            return False
        self.log.info("TX %s", line.strip())
        return not self.faults.note_sent()


def build_parser():
    parser = base_parser(__doc__.splitlines()[0])
    parser.add_argument("--control-ip", default="127.0.0.1",
                        help="control station to send telemetry to")
    parser.add_argument("--telemetry-port", type=int, default=TELEMETRY_PORT)
    parser.add_argument("--listen-ip", default="0.0.0.0")
    parser.add_argument("--command-port", type=int, default=COMMAND_PORT)
    parser.add_argument("--period", type=int, default=1,
                        help="initial integration period, seconds")
    parser.add_argument("--rate", type=float, default=500.0,
                        help="counts per second in the hottest channel")
    parser.add_argument("--beam-profile", choices=("steady", "burst", "ramp", "off"),
                        default="steady")
    parser.add_argument("--burst-period", type=float, default=20.0,
                        help="cycle length for the burst and ramp profiles")
    parser.add_argument("--burst-duty", type=float, default=0.5)
    parser.add_argument("--dead-channel", type=int, default=0, metavar="N",
                        help="pin channel N to zero (0 = none)")
    add_fault_args(parser, tcp=True)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    configure_logging(args.verbose)
    sim = DosimeterSim(args)
    banner = (f"Dosimeter simulator: commands on {args.listen_ip}:{args.command_port}, "
              f"telemetry to {args.control_ip}:{args.telemetry_port} "
              f"every {args.period}s ({args.beam_profile} beam)")
    run_until_interrupt(sim, banner)


if __name__ == "__main__":
    main()
