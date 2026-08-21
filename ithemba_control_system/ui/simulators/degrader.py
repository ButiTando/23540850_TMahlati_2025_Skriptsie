"""Degrader simulator: TCP server on 1970 for commands, TCP client to the
control station on 1971 reporting the response word every 500 ms.

Defaults imitate the firmware including its awkward parts - bare ASCII digits
with no terminator, and an "awake" status that never reaches the wire - so what
works here works on the bench. Flags relax those and inject faults.

    python -m simulators.degrader
    python -m simulators.degrader --newline --hold-awake -v
"""

import logging
import random
import socket
import threading
import time

from ._common import (LENS_NAMES, LENS_OFF, LENS_ON, LENS_UPDATING,
                      LENS_NOT_CHANGED, PS_BUSY, PS_AWAKE, PS_ERROR, PS_READY,
                      PROCESS_NAMES, FaultInjector, add_fault_args, base_parser,
                      close_quietly, configure_logging, enable_keepalive,
                      make_tcp_listener, pack_response, run_until_interrupt,
                      unpack_command)

COMMAND_PORT = 1970          # we listen here for commands
RESPONSE_PORT = 1971         # we dial out to here with responses
RESPONSE_PERIOD = 0.5        # firmware cadence, main.c
MOTION_TICK = 0.02           # lens state machine resolution
COMMAND_DIGITS = 3           # firmware copies exactly 3 bytes


class Lens:
    """One lens: where it is, where it is going, and what it reports."""

    def __init__(self, name):
        self.name = name
        self.state = LENS_OFF        # the physical truth
        self.reported = LENS_OFF     # what goes on the wire
        self.target = None           # set while a move is outstanding
        self.arrives_at = None

    @property
    def moving(self):
        return self.arrives_at is not None


class DegraderSim:
    def __init__(self, args):
        self.args = args
        self.log = logging.getLogger("degrader-sim")
        self.rng = random.Random(args.seed)
        self.faults = FaultInjector(args, self.rng)

        self.lenses = {name: Lens(name) for name in LENS_NAMES}
        for name in (args.initial_lenses or "").replace(" ", "").split(","):
            if name and name in self.lenses:
                self.lenses[name].state = self.lenses[name].reported = LENS_ON

        self.process_status = PS_READY
        self.queue = []              # lenses waiting their turn to move
        self.latched_command = None
        self._awake_until = 0.0
        self._responses_sent = 0

        self.state_lock = threading.Lock()
        self.stop_event = threading.Event()
        self.listener = None
        self.response_sock = None
        self.threads = []

    # ---------------- lifecycle ----------------

    def start(self):
        self.listener = make_tcp_listener(self.args.listen_ip, self.args.command_port)
        self.listener.settimeout(0.5)
        for name, target in (("cmd-server", self._command_loop),
                             ("responder", self._response_loop),
                             ("motion", self._motion_loop)):
            thread = threading.Thread(target=target, name=name, daemon=True)
            thread.start()
            self.threads.append(thread)

    def stop(self):
        self.stop_event.set()
        # Closing the sockets wakes any thread blocked in accept/recv/connect.
        close_quietly(self.listener)
        close_quietly(self.response_sock)
        for thread in self.threads:
            thread.join(timeout=2.0)
            if thread.is_alive():
                self.log.warning("thread %s did not stop", thread.name)

    # ---------------- command side (TCP server on 1970) ----------------

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
        """Read one command.

        The firmware commits only when the sender hangs up, which is why the
        app opens a fresh socket per command. --latch-mode immediate is for
        debugging only.
        """
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
            if self.args.latch_mode == "immediate":
                break

        raw = b"".join(chunks)[:COMMAND_DIGITS]   # firmware truncation
        digits = "".join(ch for ch in raw.decode("ascii", "ignore") if ch.isdigit())
        if not digits:
            self.log.warning("command from %s had no digits: %r", addr, raw)
            return
        self._latch_command(int(digits) & 0xFF)

    def _latch_command(self, value):
        decoded = unpack_command(value)
        with self.state_lock:
            if self.queue or any(l.moving for l in self.lenses.values()):
                # The firmware drops mid-move commands rather than queueing.
                self.log.info("busy, ignoring command %d", value)
                return

            self.latched_command = value
            if decoded["probe"]:
                # The firmware overwrites awake in the same iteration, so it
                # never reaches the wire; --hold-awake keeps it for one send.
                if self.args.hold_awake:
                    self._awake_until = time.monotonic() + RESPONSE_PERIOD * 1.2
                    self.process_status = PS_AWAKE
                self.log.info("probe received (command %d)", value)
                return

            pending = [self.lenses[name] for name in LENS_NAMES
                       if self.lenses[name].state != decoded["lenses"][name]]
            for lens in pending:
                lens.target = decoded["lenses"][lens.name]
                # Queued but not started reads as NOT CHANGED on the wire.
                lens.reported = LENS_NOT_CHANGED
            self.queue = pending
            self.process_status = PS_BUSY if pending else PS_READY
            self.log.info("command %d -> moving %s",
                          value, [l.name for l in pending] or "nothing")

    # ---------------- motion ----------------

    def _motion_loop(self):
        while not self.stop_event.wait(MOTION_TICK):
            with self.state_lock:
                self._advance()

    def _advance(self):
        now = time.monotonic()

        if self.process_status == PS_AWAKE and now >= self._awake_until:
            self.process_status = PS_READY

        if not self.queue:
            return

        lens = self.queue[0]
        if not lens.moving:
            # Serialised: the hardware has one DC motor and one shared stepper.
            travel = self.args.travel_ms / 1000.0
            if self.args.travel_jitter_ms:
                travel += self.rng.uniform(-1, 1) * self.args.travel_jitter_ms / 1000.0
            lens.arrives_at = now + max(travel, MOTION_TICK)
            lens.reported = LENS_UPDATING
            self.log.debug("%s moving, %.2fs", lens.name, lens.arrives_at - now)
            return

        if now >= lens.arrives_at:
            if lens.name == self.args.fault_lens:
                # A jammed lens never arrives; it stays UPDATING.
                self.process_status = PS_ERROR
                self.log.info("%s is faulted, holding", lens.name)
                return
            lens.state = lens.target
            lens.reported = lens.state
            lens.target = lens.arrives_at = None
            self.queue.pop(0)
            self.log.debug("%s arrived", lens.name)
            if not self.queue:
                self.process_status = PS_READY

    # ---------------- response side (TCP client to 1971) ----------------

    def _response_loop(self):
        backoff = 0.5
        while not self.stop_event.is_set():
            if self.response_sock is None:
                if not self._dial():
                    if self.stop_event.wait(backoff):
                        break
                    backoff = min(backoff * 2, 5.0)
                    continue
                backoff = 0.5
            if not self._send_response():
                close_quietly(self.response_sock)
                self.response_sock = None
                continue
            if self.stop_event.wait(self.args.interval):
                break

    def _dial(self):
        target = (self.args.control_ip, self.args.response_port)
        try:
            sock = socket.create_connection(target, timeout=3.0)
        except OSError as e:
            self.log.debug("control station %s:%d unreachable: %s", *target, e)
            return False
        enable_keepalive(sock)
        self.response_sock = sock
        self.log.info("connected to control station %s:%d", *target)
        return True

    def _send_response(self):
        with self.state_lock:
            word = pack_response([self.lenses[n].reported for n in LENS_NAMES],
                                 self.process_status)
            status = self.process_status

        if self.args.error_after and self._responses_sent == self.args.error_after:
            self.log.info("injecting an error status")
            word = pack_response([self.lenses[n].reported for n in LENS_NAMES], PS_ERROR)

        text = str(word)
        if self.faults.should_drop():
            self._responses_sent += 1
            return True
        text = self.faults.corrupt(text)

        # No terminator unless asked: the firmware sends strlen(buf) bytes.
        payload = (text + "\n" if self.args.newline else text).encode("ascii")

        try:
            for chunk in self.faults.chunks(payload):
                self.response_sock.sendall(chunk)
                if len(payload) > len(chunk):
                    time.sleep(0.003)
        except OSError as e:
            self.log.info("send failed, will redial: %s", e)
            return False

        self._responses_sent += 1
        self.log.info("TX %s (%s) %s", text, PROCESS_NAMES.get(status, status),
                      " ".join(f"{n}={self.lenses[n].reported}" for n in LENS_NAMES))
        if self.faults.note_sent():
            return False        # hang up; the app must cope with a reconnect
        return True


def build_parser():
    parser = base_parser(__doc__.splitlines()[0])
    parser.add_argument("--control-ip", default="127.0.0.1",
                        help="control station to send responses to (rig: 192.168.7.1)")
    parser.add_argument("--response-port", type=int, default=RESPONSE_PORT)
    parser.add_argument("--listen-ip", default="0.0.0.0")
    parser.add_argument("--command-port", type=int, default=COMMAND_PORT)
    parser.add_argument("--interval", type=float, default=RESPONSE_PERIOD,
                        help="seconds between responses")
    parser.add_argument("--travel-ms", type=int, default=2500,
                        help="how long one lens takes to move")
    parser.add_argument("--travel-jitter-ms", type=int, default=300)
    parser.add_argument("--initial-lenses", default="",
                        help="comma-separated lenses that start in the beam")
    parser.add_argument("--fault-lens", default=None, choices=LENS_NAMES,
                        help="this lens jams mid-move and the status goes to error")
    parser.add_argument("--error-after", type=int, default=0, metavar="N",
                        help="report an error status on the Nth response")
    parser.add_argument("--newline", action="store_true",
                        help="terminate responses with a newline (firmware does not)")
    parser.add_argument("--hold-awake", action="store_true",
                        help="hold the awake status long enough for the UI to see it "
                             "(the firmware overwrites it before it reaches the wire)")
    parser.add_argument("--latch-mode", choices=("on-close", "immediate"),
                        default="on-close",
                        help="when a command takes effect; the firmware latches on close")
    add_fault_args(parser, tcp=True)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    configure_logging(args.verbose)
    sim = DegraderSim(args)
    banner = (f"Degrader simulator: commands on {args.listen_ip}:{args.command_port}, "
              f"responses to {args.control_ip}:{args.response_port} "
              f"every {args.interval}s ({'newline' if args.newline else 'no'} terminator)")
    run_until_interrupt(sim, banner)


if __name__ == "__main__":
    main()
