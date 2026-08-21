#!/usr/bin/env python3
"""End-to-end: the real Degrader client against the degrader simulator.

Uses spare ports so it never competes with a running control station.
"""

import sys
import threading
import time

from _bootstrap import Results, wait_for  # noqa: E402

import radiation_systems.degrader as client
from simulators._common import (LENS_NAMES, LENS_UPDATING, PS_AWAKE, PS_BUSY,
                                PS_READY)
from simulators.degrader import DegraderSim, build_parser

COMMAND_PORT = 21970
RESPONSE_PORT = 21971

# Small values need a gap to be framed, so do not drive this faster.
INTERVAL = "0.5"


def with_sim(argv, body):
    """Run one simulator + one client, then tear both down."""
    args = build_parser().parse_args(
        argv + ["--command-port", str(COMMAND_PORT),
                "--response-port", str(RESPONSE_PORT)])
    sim = DegraderSim(args)
    sim.start()
    time.sleep(0.2)
    degrader = client.Degrader(testing=True, listen_port=RESPONSE_PORT,
                               command_port=COMMAND_PORT)
    time.sleep(0.8)
    try:
        body(sim, degrader)
    finally:
        degrader.degrader_close()
        sim.stop()
        time.sleep(0.3)


def watch(degrader, stop, seen):
    """Sample the decoded state, to catch transient values."""
    while not stop.is_set():
        if degrader.resp.process_status == PS_BUSY:
            seen["busy"] = True
        if degrader.resp.lens_status_6mm == LENS_UPDATING:
            seen["updating"] = True
        time.sleep(0.02)


def main():
    r = Results()

    # No terminator at all: the case the old framing could not handle.
    def firmware_framing(sim, degrader):
        ok = wait_for(lambda: degrader.resp.process_status == PS_READY
                      and degrader.resp.response != 0)
        r.check("firmware framing (no terminator) decoded", ok,
                f"(response={degrader.resp.response})")
    with_sim(["--travel-ms", "200", "--initial-lenses", "6mm",
              "--interval", INTERVAL], firmware_framing)

    def newline_framing(sim, degrader):
        ok = wait_for(lambda: degrader.resp.process_status == PS_READY
                      and degrader.resp.lens_status_6mm == 1)
        r.check("newline framing gives the same result", ok,
                f"(response={degrader.resp.response})")
    with_sim(["--newline", "--travel-ms", "200", "--initial-lenses", "6mm",
              "--interval", INTERVAL], newline_framing)

    # A full move: queued -> moving -> settled, status following.
    def full_move(sim, degrader):
        wait_for(lambda: degrader.resp.process_status == PS_READY)
        seen = {"busy": False, "updating": False}
        stop = threading.Event()
        threading.Thread(target=watch, args=(degrader, stop, seen),
                         daemon=True).start()

        degrader.set_command({name: {"desired_state": 1 if name == "6mm" else 0}
                              for name in LENS_NAMES})
        degrader.send_command()
        settled = wait_for(lambda: degrader.resp.lens_status_6mm == 1
                           and degrader.resp.process_status == PS_READY)
        stop.set()

        r.check("simulator latched the command", sim.latched_command == 0b100,
                f"(got {sim.latched_command})")
        r.check("status went busy during the move", seen["busy"])
        r.check("lens reported UPDATING while moving", seen["updating"])
        r.check("settled to the commanded state", settled,
                f"(6mm={degrader.resp.lens_status_6mm} "
                f"status={degrader.resp.process_status})")
    with_sim(["--travel-ms", "2000", "--travel-jitter-ms", "0",
              "--interval", INTERVAL], full_move)

    # Awake is only observable when the simulator holds it.
    def probe(sim, degrader):
        wait_for(lambda: degrader.resp.process_status == PS_READY)
        saw = {"awake": False}
        stop = threading.Event()

        def sample():
            while not stop.is_set():
                if degrader.resp.process_status == PS_AWAKE:
                    saw["awake"] = True
                time.sleep(0.02)
        threading.Thread(target=sample, daemon=True).start()
        degrader.start_degrader(None, "127.0.0.1")
        time.sleep(1.5)
        stop.set()
        r.check("--hold-awake makes the probe ack visible", saw["awake"])
    with_sim(["--hold-awake", "--travel-ms", "200", "--interval", INTERVAL], probe)

    def faults(sim, degrader):
        ok = wait_for(lambda: degrader.resp.process_status == PS_READY, timeout=12)
        r.check("survives split writes and garbage", ok,
                f"(status={degrader.resp.process_status})")
    with_sim(["--split-writes", "--garbage-rate", "0.3", "--travel-ms", "200",
              "--interval", INTERVAL, "--seed", "3",
              "--initial-lenses", "2mm"], faults)

    def reconnects(sim, degrader):
        wait_for(lambda: degrader.resp.process_status == PS_READY)
        before = sim._responses_sent
        ok = wait_for(lambda: sim._responses_sent > before + 8, timeout=15)
        r.check("keeps receiving across forced disconnects",
                ok and degrader.resp.process_status == PS_READY)
    with_sim(["--drop-connection", "3", "--travel-ms", "200",
              "--interval", INTERVAL, "--initial-lenses", "3mm"], reconnects)

    return r.finish()


if __name__ == "__main__":
    sys.exit(main())
