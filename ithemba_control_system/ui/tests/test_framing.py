#!/usr/bin/env python3
"""Unit tests for ResponseFramer: recovering messages from an unframed stream.

The degrader sends bare digits with no terminator, so this is the part of the
client most likely to break silently. No sockets involved.
"""

import random
import sys

from _bootstrap import Results  # noqa: E402  (path setup happens on import)

from radiation_systems.degrader import ResponseFramer
from simulators._common import LENS_NAMES, pack_response


def main():
    r = Results()

    # (input, expected values, digits still held).  '|' marks a segment break.
    cases = [
        ("49152",          [49152],        ""),      # framed instantly
        ("491|52",         [49152],        ""),      # split across segments
        ("4915249152",     [49152, 49152], ""),      # two messages, no separator
        ("049152",         [0, 49152],     ""),      # '0' cannot be a prefix
        ("49152\n49152\n", [49152, 49152], ""),      # newline-terminated peer
        ("49152\r\n",      [49152],        ""),
        ("  49152 ",       [49152],        ""),
        ("JUNK49152",      [49152],        ""),      # garbage acts as a separator
        ("65535",          [65535],        ""),      # the maximum value
        ("65539",          [6553],         "9"),     # would overflow, so it split
        ("4369",           [],             "4369"),  # small: waits for a gap
        ("0",              [],             "0"),
    ]
    for data, expected, remainder in cases:
        framer = ResponseFramer()
        got = []
        for segment in data.split("|"):
            got += framer.feed(segment)
        r.check(f"frames {data!r}", got == expected and framer._digits == remainder,
                f"-> {got} held={framer._digits!r}")

    # Feeding one byte at a time must give the same answer as feeding it whole.
    blob = "49152\n0\n655354369x12288"
    bulk = ResponseFramer()
    drip = ResponseFramer()
    bulk_out = bulk.feed(blob) + bulk.flush()
    drip_out = []
    for char in blob:
        drip_out += drip.feed(char)
    drip_out += drip.flush()
    r.check("byte-at-a-time matches bulk", bulk_out == drip_out,
            f"{bulk_out} vs {drip_out}")

    # Values stay legal and the buffer bounded, whatever arrives.
    rng = random.Random(7)
    framer = ResponseFramer()
    violations = 0
    for _ in range(20000):
        chunk = "".join(rng.choice("0123456789\n xJ")
                        for _ in range(rng.randint(1, 8)))
        values = framer.feed(chunk)
        if rng.random() < 0.1:
            values += framer.flush()
        violations += sum(1 for v in values if not 0 <= v <= 65535)
        if len(framer._digits) > 4:
            violations += 1
    r.check("fuzz: values legal and buffer bounded", violations == 0,
            f"({violations} violations)")

    # The simulator restates the bit layout; if they disagree, one drifted.
    from radiation_systems.degrader import Response
    rng = random.Random(0)
    mismatches = 0
    for _ in range(5000):
        states = [rng.randint(0, 3) for _ in LENS_NAMES]
        status = rng.randint(0, 3)
        response = Response(0)
        for name, state in zip(LENS_NAMES, states):
            setattr(response, f"lens_status_{name}", state)
        response.process_status = status
        if response.response != pack_response(states, status):
            mismatches += 1
    r.check("app and simulator agree on the response layout", mismatches == 0,
            f"({mismatches} mismatches)")

    return r.finish()


if __name__ == "__main__":
    sys.exit(main())
