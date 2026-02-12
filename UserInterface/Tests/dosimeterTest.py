#!/usr/bin/env python3
"""
Dosimeter UDP Simulator Server

- Receives control packets on 0.0.0.0:1972
- Sends CSV telemetry to CONTROL_STATION_IP_ADDR:1973
- CSV line format matches Dosimeter.parse_csv_string()

Command protocol:
- 4-byte big-endian words.
- If top 8 bits (command) != 0x00:
    0x10 -> SET_PERIOD (data = seconds, 1..3600)
- If top 8 bits == 0x00:
    Interpret as date or time:
      If byte2 (hour/year) > 23 -> treat as DATE: (year, month, day)
      Else                      -> treat as TIME: (hour, minute, second)
"""

import argparse
import logging
import random
import socket
import struct
import threading
import time
from datetime import datetime, timedelta, timezone

SEND_TO_DOSIMETER_PORT = 1972           # server listens here for control
RECV_FROM_DOSIMETER_PORT = 1973         # server sends telemetry here

DEFAULT_CONTROL_IP = "192.168.7.1"      # override with --control-ip
DEFAULT_DEVICE_IP  = "192.168.7.1"

# Command codes
CMD_SET_PERIOD = 0x10

# Telemetry channels
CHANNEL_LABELS = ["channel1", "channel2", "channel3", "channel4", "channel5", "channel6"]


class DosimeterServer:
    def __init__(self,
                 control_ip: str,
                 control_port: int = RECV_FROM_DOSIMETER_PORT,
                 listen_ip: str = DEFAULT_DEVICE_IP,
                 listen_port: int = SEND_TO_DOSIMETER_PORT,
                 period_sec: int = 1,
                 seed: int | None = None):
        self.control_addr = (control_ip, control_port)
        self.listen_addr = (listen_ip, listen_port)
        self.period_sec = max(1, period_sec)
        self.stop_event = threading.Event()
        self.log = logging.getLogger("dosimeter-server")
        self.sock_rx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock_tx = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock_rx.bind(self.listen_addr)
        self.log.info("Listening for control on %s:%d", *self.listen_addr)

        # Clock handling: allow “set date/time” by keeping an offset to system clock
        self._clock_offset = timedelta(0)  # simulated_device_time = now() + offset

        # RNG for counts
        self.rng = random.Random(seed)

        # rolling baseline per channel to look less synthetic
        self._channel_baselines = [self.rng.randint(40, 100000) for _ in CHANNEL_LABELS]

        # Send startup header
        self._send_header_once()

    # ---------- public control ----------

    def start(self):
        self.rx_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self.tx_thread = threading.Thread(target=self._tx_loop, daemon=True)
        self.rx_thread.start()
        self.tx_thread.start()

    def stop(self):
        self.stop_event.set()
        try:
            self.rx_thread.join(timeout=1.0)
            self.tx_thread.join(timeout=1.0)
        except Exception:
            pass
        self.sock_rx.close()
        self.sock_tx.close()

    # ---------- loops ----------

    def _rx_loop(self):
        """Receive 4B control words; act accordingly; send ACKs as text."""
        while not self.stop_event.is_set():
            try:
                self.sock_rx.settimeout(0.5)
                data, addr = self.sock_rx.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                break

            if len(data) < 4:
                self._send_text(f"WARN short packet ({len(data)}B) from {addr}")
                continue

            # Process in 4-byte chunks
            for i in range(0, len(data), 4):
                word = data[i:i+4]
                if len(word) < 4:
                    self._send_text("WARN trailing bytes ignored")
                    break
                cmd_or_zero = word[0]
                v = struct.unpack(">I", word)[0]

                if cmd_or_zero != 0x00:
                    # CommandPacket: [cmd | 24-bit data]
                    cmd = (v >> 24) & 0xFF
                    dat = v & 0xFFFFFF
                    self._handle_command(cmd, dat)
                else:
                    # 0x00 leading byte means Date/Time word from helpers
                    b2 = (v >> 16) & 0xFF  # could be year (date) or hour (time)
                    b3 = (v >> 8) & 0xFF
                    b4 = (v >> 0) & 0xFF
                    if b2 > 23:
                        self._handle_set_date(b2, b3, b4)  # year, month, day
                    else:
                        self._handle_set_time(b2, b3, b4)  # hour, minute, second

    def _tx_loop(self):
        """Periodic CSV telemetry to control station."""
        # initial slight delay to let control connect
        next_t = time.monotonic() + 0.1
        while not self.stop_event.is_set():
            now = time.monotonic()
            if now >= next_t:
                self._send_csv_line()
                next_t = now + self.period_sec
            # small sleep to reduce CPU
            time.sleep(0.05)

    # ---------- command handlers ----------

    def _handle_command(self, cmd: int, data: int):
        if cmd == CMD_SET_PERIOD:
            new_period = max(1, min(int(data), 3600))
            old = self.period_sec
            self.period_sec = new_period
            self._send_text(f"ACK SET_PERIOD {old}→{new_period}s")
        else:
            self._send_text(f"WARN unknown_cmd 0x{cmd:02X} data=0x{data:06X}")

    def _handle_set_date(self, year: int, month: int, day: int):
        # Use current simulated time’s HMS, set Y-M-D
        cur = self._now_sim()
        try:
            # Interpret 0–255 year as (2000 + year) if < 100, else 1900+?
            # Keep it simple: map 0..255 to 2000..2255 (customizable)
            y_abs = 2000 + (year & 0xFF)
            new_dt = cur.replace(year=y_abs, month=max(1, min(month, 12)), day=max(1, min(day, 31)))
        except ValueError:
            # Clamp day within month
            for dd in range(28, 32):
                try:
                    new_dt = cur.replace(year=y_abs, month=max(1, min(month, 12)), day=dd)
                    break
                except ValueError:
                    continue
        self._set_sim_time(new_dt)
        self._send_text(f"ACK SET_DATE {new_dt.isoformat(sep=' ')}")

    def _handle_set_time(self, hour: int, minute: int, second: int):
        hour = max(0, min(hour, 23))
        minute = max(0, min(minute, 59))
        second = max(0, min(second, 59))
        cur = self._now_sim()
        new_dt = cur.replace(hour=hour, minute=minute, second=second, microsecond=0)
        self._set_sim_time(new_dt)
        self._send_text(f"ACK SET_TIME {new_dt.isoformat(sep=' ')}")

    # ---------- telemetry ----------

    def _send_header_once(self):
        header_lines = [
            "# Dosimeter Telemetry Simulator v1",
            "# System: 6-channel gamma dose counter (simulated)",
            "# Channels: " + ",".join(CHANNEL_LABELS),
            "# CSV: YYYY-MM-DD HH:MM:SS,c1,c2,c3,c4,c5,c6",
        ]
        for line in header_lines:
            self._send_text(line)

    def _send_csv_line(self):
        dt = self._now_sim()
        # slightly wandering baselines + Poisson-like noise
        counts = []
        for i, base in enumerate(self._channel_baselines):
            # wander
            self._channel_baselines[i] = max(5, base + self.rng.randint(-1, 1))
            # noise
            val = int(max(0, self.rng.gauss(self._channel_baselines[i], self._channel_baselines[i] ** 0.5)))
            counts.append(val)

        line = "{:%Y-%m-%d %H:%M:%S},{}\n".format(dt, ",".join(str(c) for c in counts))
        self.sock_tx.sendto(line.encode("ascii"), self.control_addr)
        self.log.info("TX %s -> %s:%d", line.strip(), *self.control_addr)

    def _send_text(self, s: str):
        msg = (s if s.endswith("\n") else s + "\n").encode("utf-8", errors="ignore")
        self.sock_tx.sendto(msg, self.control_addr)
        self.log.debug("TX %s -> %s:%d", s.strip(), *self.control_addr)

    # ---------- simulated clock ----------

    def _now_sim(self) -> datetime:
        return datetime.now(timezone.utc) + self._clock_offset

    def _set_sim_time(self, target_dt: datetime):
        real_now = datetime.now(timezone.utc)
        # Preserve timezone of target_dt if provided; otherwise treat as UTC
        if target_dt.tzinfo is None:
            target_dt = target_dt.replace(tzinfo=timezone.utc)
        self._clock_offset = target_dt - real_now


def main():
    parser = argparse.ArgumentParser(description="Dosimeter UDP Simulator Server")
    parser.add_argument("--control-ip", default=DEFAULT_CONTROL_IP,
                        help="Control station IP to send telemetry/ACKs to (default: %(default)s)")
    parser.add_argument("--control-port", type=int, default=RECV_FROM_DOSIMETER_PORT,
                        help="Control station UDP port to send telemetry/ACKs to (default: %(default)s)")
    parser.add_argument("--listen-ip", default=DEFAULT_DEVICE_IP,
                        help="Local IP to bind for receiving control (default: %(default)s)")
    parser.add_argument("--listen-port", type=int, default=SEND_TO_DOSIMETER_PORT,
                        help="Local UDP port for control (default: %(default)s)")
    parser.add_argument("--period", type=int, default=1, help="Initial sample period seconds (default: 1)")
    parser.add_argument("--seed", type=int, default=None, help="Random seed for deterministic counts")
    parser.add_argument("-v", "--verbose", action="count", default=0, help="Increase logging verbosity")
    args = parser.parse_args()

    lvl = logging.WARNING
    if args.verbose == 1:
        lvl = logging.INFO
    elif args.verbose >= 2:
        lvl = logging.DEBUG
    logging.basicConfig(level=lvl, format="%(asctime)s %(levelname)s: %(message)s")

    srv = DosimeterServer(
        control_ip=args.control_ip,
        control_port=args.control_port,
        listen_ip=args.listen_ip,
        listen_port=args.listen_port,
        period_sec=args.period,
        seed=args.seed,
    )
    try:
        srv.start()
        print(f"Simulator running. RX control on {args.listen_ip}:{args.listen_port} → "
              f"TX to {args.control_ip}:{args.control_port}. Press Ctrl+C to stop.")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping…")
    finally:
        srv.stop()


if __name__ == "__main__":
    main()
