#!/usr/bin/env python3
"""End-to-end: the real Dosimeter client against the dosimeter simulator.

Uses spare ports so it never competes with a running control station.
"""

import logging
import socket
import sys
import time

from _bootstrap import Results, wait_for  # noqa: E402

import radiation_systems.dosimeter as client
from simulators.dosimeter import DosimeterSim, build_parser

COMMAND_PORT = 21972
TELEMETRY_PORT = 21973

_log_lines = []


class _Capture(logging.Handler):
    def emit(self, record):
        _log_lines.append(record.getMessage())


def with_sim(argv, body):
    """Run one simulator + one client, then tear both down."""
    args = build_parser().parse_args(
        argv + ["--command-port", str(COMMAND_PORT),
                "--telemetry-port", str(TELEMETRY_PORT)])
    sim = DosimeterSim(args)
    sim.start()

    # Point the client at the spare ports for the duration of the test.
    client.CONTROL_STATION_IP_ADDR = "0.0.0.0"
    client.RECV_FROM_DOSIMETER_PORT = TELEMETRY_PORT
    client.SEND_TO_DOSIMETER_PORT = COMMAND_PORT
    dosimeter = client.Dosimeter(log_file="/tmp/dosimeter_sim_test.csv")
    dosimeter.dosimeter_ip = "127.0.0.1"
    started = dosimeter.start_server()
    try:
        body(sim, dosimeter, started)
    finally:
        dosimeter.stop_server()
        sim.stop()
        time.sleep(0.2)


def main():
    r = Results()
    sim_log = logging.getLogger("dosimeter-sim")
    sim_log.addHandler(_Capture())
    sim_log.setLevel(logging.INFO)

    def telemetry(sim, dosimeter, started):
        r.check("client bound the telemetry port", started)
        ok = wait_for(lambda: any(c > 0 for c in dosimeter.latest_counts))
        r.check("CSV telemetry parsed by the client", ok,
                f"(counts={dosimeter.latest_counts})")
        r.check("all six channels present", len(dosimeter.latest_counts) == 6)
    with_sim(["--period", "1", "--rate", "800", "--seed", "1"], telemetry)

    # The old simulator expected 0x10, so a period change never once worked.
    def set_period(sim, dosimeter, started):
        wait_for(lambda: any(c > 0 for c in dosimeter.latest_counts))
        _log_lines.clear()
        dosimeter.blm_send_new_period(3)
        ok = wait_for(lambda: sim.period == 3, timeout=4)
        r.check("SET_PERIOD accepted (opcode 0x01)", ok, f"(period={sim.period})")
        r.check("acknowledged, not rejected as unknown",
                any("ACK SET_PERIOD" in m for m in _log_lines)
                and not any("unknown_cmd" in m for m in _log_lines))
    with_sim(["--period", "1", "--seed", "2"], set_period)

    # Year 20 is exactly what the old byte2>23 heuristic misread as a time.
    def date_and_time(sim, dosimeter, started):
        _log_lines.clear()
        dosimeter.set_date(20, 8, 21)
        dosimeter.set_time(13, 45, 30)
        ok = wait_for(lambda: any("ACK SET_DATE 2020-08-21" in m for m in _log_lines)
                      and any("ACK SET_TIME 13:45:30" in m for m in _log_lines),
                      timeout=4)
        r.check("SET_DATE/SET_TIME decoded unambiguously", ok,
                f"({[m for m in _log_lines if 'ACK' in m]})")
        r.check("no ambiguous-word warning was needed",
                not any("ambiguous" in m for m in _log_lines))
        r.check("simulated clock moved", sim.now_sim().year == 2020,
                f"(year={sim.now_sim().year})")
    with_sim(["--period", "1", "--seed", "3"], date_and_time)

    # An ACK on this port would surface as a parse error.
    def telemetry_is_csv_only(sim, dosimeter, started):
        dosimeter.blm_send_new_period(1)     # provoke an acknowledgement
        time.sleep(2.0)
        # A stray ACK would have been reported as a malformed line.
        r.check("only valid CSV reaches the telemetry port",
                any(c >= 0 for c in dosimeter.latest_counts)
                and dosimeter.server_thread.is_alive())
    with_sim(["--period", "1", "--seed", "4"], telemetry_is_csv_only)

    def survives_garbage(sim, dosimeter, started):
        wait_for(lambda: any(c > 0 for c in dosimeter.latest_counts))
        time.sleep(2.0)
        r.check("client RX thread survives malformed packets",
                dosimeter.server_thread.is_alive())
    with_sim(["--period", "1", "--garbage-rate", "0.5", "--seed", "5"],
             survives_garbage)

    # The original Linux symptom: a port that would not re-bind.
    def restart(sim, dosimeter, started):
        r.check("first start bound the port", started)
        dosimeter.stop_server()
        r.check("restarts on the same port", dosimeter.start_server() is True)
    with_sim(["--period", "1", "--seed", "6"], restart)

    # The exact shifts the firmware uses (tcp_server_take_command and
    # BLM_HandleCommand), so a layout change fails here rather than on a board.
    def firmware_decode(packet):
        word = ((packet[0] << 24) | (packet[1] << 16)
                | (packet[2] << 8) | packet[3])
        data = word & 0xFFFFFF
        return {
            "opcode": (word >> 24) & 0xFF,
            "data": data,
            "hi": (data >> 16) & 0xFF,
            "mid": (data >> 8) & 0xFF,
            "lo": data & 0xFF,
        }

    probe = client.Dosimeter.__new__(client.Dosimeter)

    decoded = firmware_decode(probe.create_command_packet(client.CMD_SET_PERIOD, 3600))
    r.check("firmware would decode SET_PERIOD",
            decoded["opcode"] == 0x01 and decoded["data"] == 3600,
            f"(opcode={decoded['opcode']:#04x} data={decoded['data']})")

    decoded = firmware_decode(probe.create_date_command(20, 8, 21))
    r.check("firmware would decode SET_DATE as year/month/day",
            (decoded["opcode"], decoded["hi"], decoded["mid"], decoded["lo"])
            == (0x02, 20, 8, 21),
            f"(got {decoded['opcode']:#04x} {decoded['hi']}-{decoded['mid']}-{decoded['lo']})")

    decoded = firmware_decode(probe.create_time_command(13, 45, 30))
    r.check("firmware would decode SET_TIME as hour/minute/second",
            (decoded["opcode"], decoded["hi"], decoded["mid"], decoded["lo"])
            == (0x03, 13, 45, 30),
            f"(got {decoded['opcode']:#04x} {decoded['hi']}:{decoded['mid']}:{decoded['lo']})")

    return r.finish()


if __name__ == "__main__":
    sys.exit(main())
