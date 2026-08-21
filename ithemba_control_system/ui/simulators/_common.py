"""Scaffolding shared by the device simulators: CLI, logging, faults."""

import argparse
import importlib.util
import logging
import pathlib
import random
import signal
import time


def _load_net_utils():
    """Load net_utils.py without running the package __init__, which pulls in
    OpenCV. sys.path is no good either: platform.py would shadow the stdlib.
    """
    path = pathlib.Path(__file__).resolve().parent.parent / "radiation_systems" / "net_utils.py"
    spec = importlib.util.spec_from_file_location("_sim_net_utils", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


net_utils = _load_net_utils()
make_tcp_listener = net_utils.make_tcp_listener
make_udp_socket = net_utils.make_udp_socket
local_lan_ip = net_utils.local_lan_ip
enable_keepalive = net_utils.enable_keepalive
close_quietly = net_utils.close_quietly


# Restated rather than imported: sharing one implementation would hide the
# layout regressions these tests exist to catch. test_framing.py cross-checks.

# Bit 0 is the 2 mm lens, bit 7 the probe flag; same order for the response.
LENS_NAMES = ("2mm", "3mm", "6mm", "8mm", "10mm", "12mm", "30mm")

LENS_OFF, LENS_ON, LENS_UPDATING, LENS_NOT_CHANGED = 0, 1, 2, 3

# degraderProcessStatus: 3 is the resting state, 2 is the error.
PS_BUSY, PS_AWAKE, PS_ERROR, PS_READY = 0, 1, 2, 3

PROCESS_NAMES = {PS_BUSY: "busy", PS_AWAKE: "awake",
                 PS_ERROR: "error", PS_READY: "ready"}
LENS_STATE_NAMES = {LENS_OFF: "off", LENS_ON: "on",
                    LENS_UPDATING: "updating", LENS_NOT_CHANGED: "unchanged"}


def pack_response(lens_states, process_status):
    """Build the 16-bit response word from seven 2-bit lens fields."""
    word = (process_status & 0b11) << 14
    for index, state in enumerate(lens_states):
        word |= (state & 0b11) << (2 * index)
    return word


def unpack_command(word):
    """Split the 8-bit command byte into per-lens bits plus the probe flag."""
    return {
        "lenses": {name: (word >> index) & 1 for index, name in enumerate(LENS_NAMES)},
        "probe": (word >> 7) & 1,
    }


def base_parser(description):
    """Argparse scaffolding common to every simulator."""
    parser = argparse.ArgumentParser(
        description=description,
        formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--seed", type=int, default=None,
                        help="RNG seed, for reproducible runs")
    parser.add_argument("-v", "--verbose", action="count", default=0,
                        help="-v for per-message logging, -vv for debug")
    return parser


def add_fault_args(parser, tcp=True):
    """Add the opt-in misbehaviour flags; all default to well-behaved."""
    group = parser.add_argument_group(
        "fault injection",
        "Deliberate misbehaviour, for testing how the app copes. Off by default.")
    group.add_argument("--drop-rate", type=float, default=0.0, metavar="P",
                       help="drop this fraction of outgoing messages (0.0-1.0)")
    group.add_argument("--garbage-rate", type=float, default=0.0, metavar="P",
                       help="replace this fraction of messages with non-numeric junk")
    group.add_argument("--stall", type=float, default=0.0, metavar="SECONDS",
                       help="go silent this long after every 20 messages")
    if tcp:
        group.add_argument("--split-writes", action="store_true",
                           help="dribble each message out in small pieces")
        group.add_argument("--drop-connection", type=int, default=0, metavar="N",
                           help="hang up after every N messages (0 = never)")
    return parser


class FaultInjector:
    """Decides how, and whether, a message gets mangled on its way out."""

    GARBAGE = ["not-a-number", "???", "NaN", "<html>", "-1e9", ""]

    def __init__(self, args, rng=None):
        self.drop_rate = getattr(args, "drop_rate", 0.0)
        self.garbage_rate = getattr(args, "garbage_rate", 0.0)
        self.split_writes = getattr(args, "split_writes", False)
        self.drop_connection = getattr(args, "drop_connection", 0)
        self.stall = getattr(args, "stall", 0.0)
        self.rng = rng or random.Random(getattr(args, "seed", None))
        self.sent = 0
        self.log = logging.getLogger("faults")

    @property
    def active(self):
        return bool(self.drop_rate or self.garbage_rate or self.split_writes
                    or self.drop_connection or self.stall)

    def should_drop(self):
        if self.drop_rate and self.rng.random() < self.drop_rate:
            self.log.info("dropping a message")
            return True
        return False

    def corrupt(self, text):
        """Swap a well-formed message for something unparseable."""
        if self.garbage_rate and self.rng.random() < self.garbage_rate:
            junk = self.rng.choice(self.GARBAGE)
            self.log.info("corrupting %r -> %r", text, junk)
            return junk
        return text

    def chunks(self, payload):
        """Split a payload so the receiver has to reassemble it."""
        if not self.split_writes or len(payload) < 2:
            return [payload]
        cut = self.rng.randint(1, len(payload) - 1)
        return [payload[:cut], payload[cut:]]

    def note_sent(self):
        """Count a message; returns True if the link should now be dropped."""
        self.sent += 1
        if self.stall and self.sent % 20 == 0:
            self.log.info("stalling for %.1fs", self.stall)
            time.sleep(self.stall)
        if self.drop_connection and self.sent % self.drop_connection == 0:
            self.log.info("dropping the connection after %d messages", self.sent)
            return True
        return False


def configure_logging(verbosity):
    level = logging.WARNING
    if verbosity == 1:
        level = logging.INFO
    elif verbosity >= 2:
        level = logging.DEBUG
    logging.basicConfig(level=level, format="%(asctime)s %(levelname)s %(name)s: %(message)s")


def run_until_interrupt(sim, banner):
    """Run a simulator until Ctrl-C, then stop it cleanly."""
    stopping = {"now": False}

    def handle(signum, frame):
        stopping["now"] = True

    signal.signal(signal.SIGINT, handle)
    signal.signal(signal.SIGTERM, handle)

    sim.start()
    print(banner)
    print("Press Ctrl+C to stop.")
    try:
        while not stopping["now"]:
            time.sleep(0.2)
    finally:
        print("\nStopping ...")
        sim.stop()
        print("Stopped.")
