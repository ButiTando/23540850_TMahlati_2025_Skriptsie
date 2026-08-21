import struct
import socket
import threading
import time
from datetime import datetime
from typing import Dict, Optional

from .net_utils import make_tcp_listener, enable_keepalive, close_quietly

# ----------------------------------------------------------------------
# Configuration constants
# ----------------------------------------------------------------------
# The dosimeter is a TCP server for commands and a TCP client for telemetry.
SEND_TO_DOSIMETER_PORT = 1972      # Commands → dosimeter (we connect)
RECV_FROM_DOSIMETER_PORT = 1973    # Data ← dosimeter (we listen)
CONNECT_TIMEOUT = 3.0
RECV_TIMEOUT = 10.0
CONTROL_STATION_IP_ADDR = "0.0.0.0"   # every interface, not just loopback
DOSIMETER_DEFAULT_IP = "192.168.7.105"    # dosimeter

# Command codes (8-bit) – must match the firmware
CMD_SET_PERIOD = 0x01
CMD_SET_DATE   = 0x02
CMD_SET_TIME   = 0x03

# ----------------------------------------------------------------------
# Dosimeter control & logging class
# ----------------------------------------------------------------------
class Dosimeter:
    """
    A full-featured controller for a 6-channel dosimeter.

    Features
    --------
    * TCP server that receives CSV lines (timestamp + 6 counts)
    * Configurable integration period (seconds)
    * Automatic CSV logging with proper heading
    * Helper methods to send period / date / time commands
    """

    # ------------------------------------------------------------------
    # Construction / housekeeping
    # ------------------------------------------------------------------
    def __init__(self, log_file: str = "dosimeter_log.csv", period_seconds: int = 1):

        self.log_file_handle = None
        self.current_log_filename = None
        self.logging_lock = threading.Lock()

        self.log_file = log_file
        self.period = period_seconds


        # ---- NEW: shared state for the UI ----
        self.dosimeter_ip = DOSIMETER_DEFAULT_IP
        self.latest_counts = [0] * 6          # most-recent per-channel counts
        self.counts_lock = threading.Lock()   # protects latest_counts

        # Opened in start_server(), not here: a port held by a previous run
        # would otherwise take the whole UI down at startup.
        self.listen_sock: Optional[socket.socket] = None
        self.client_sock: Optional[socket.socket] = None

        # Threading
        self.server_thread: Optional[threading.Thread] = None
        self.running = False

        # Print system description
        self._print_system_description()

        # Initialise CSV file (header only once)
        self._init_csv()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def start_server(self) -> bool:
        """Start the UDP listener in a background thread.

        Returns True if the listener is up.  Safe to call again after
        stop_server(): fresh sockets are created each time.
        """
        if self.running:
            print("[Dosimeter] Server already running.")
            return True

        try:
            self.listen_sock = make_tcp_listener(CONTROL_STATION_IP_ADDR,
                                                 RECV_FROM_DOSIMETER_PORT)
        except OSError as e:
            print(f"[Dosimeter] Could not bind {CONTROL_STATION_IP_ADDR}:"
                  f"{RECV_FROM_DOSIMETER_PORT} - {e}")
            self.listen_sock = None
            return False

        self.listen_sock.settimeout(1.0)   # keeps the loop responsive to shutdown

        self.running = True
        self.server_thread = threading.Thread(target=self._server_loop, daemon=True)
        self.server_thread.start()
        print(f"[Dosimeter] TCP server listening on {CONTROL_STATION_IP_ADDR}:{RECV_FROM_DOSIMETER_PORT}")
        return True

    def stop_server(self) -> None:
        """Stop the UDP listener and release the port."""
        self.running = False
        if self.server_thread:
            self.server_thread.join(timeout=2.0)
            self.server_thread = None
        # Dropped, not reused, so start_server() can reconnect later.
        close_quietly(self.client_sock)
        close_quietly(self.listen_sock)
        self.client_sock = None
        self.listen_sock = None
        print("[Dosimeter] Server stopped.")

    def _send(self, packet: bytes, what: str) -> bool:
        """Send one command; a fresh connection each time, as the degrader does."""
        if not self.dosimeter_ip:
            print(f"[Dosimeter] No IP set - cannot send {what}")
            return False
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.settimeout(CONNECT_TIMEOUT)
                sock.connect((self.dosimeter_ip, SEND_TO_DOSIMETER_PORT))
                sock.sendall(packet)
            return True
        except OSError as e:
            print(f"[Dosimeter] Failed to send {what}: {e}")
            return False

    def blm_send_new_period(self, new_period: int) -> None:
        """
        Change the integration period on the dosimeter.

        Args:
            new_period: Integration time in seconds (24-bit unsigned).
        """
        if not (0 <= new_period <= 0xFFFFFF):
            raise ValueError("Period must be 0–16777215 seconds")
        packet = self.create_command_packet(CMD_SET_PERIOD, new_period)
        if self._send(packet, "period"):
            self.period = new_period
            print(f"[Dosimeter] Sent new period = {new_period} s")

    def set_date(self, year: int, month: int, day: int) -> None:
        """Send a DATE command (8-bit fields)."""
        packet = self.create_date_command(year, month, day)
        if self._send(packet, "date"):
            print(f"[Dosimeter] Sent DATE {year:02d}-{month:02d}-{day:02d}")

    def set_time(self, hour: int, minute: int, second: int) -> None:
        """Send a TIME command (8-bit fields)."""
        packet = self.create_time_command(hour, minute, second)
        if self._send(packet, "time"):
            print(f"[Dosimeter] Sent TIME {hour:02d}:{minute:02d}:{second:02d}")

    def set_log_file(self, file_handle, filename):
        """Called by UI when a new log file is opened."""
        with self.logging_lock:
            self.log_file_handle = file_handle
            self.current_log_filename = filename

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    def _print_system_description(self) -> None:
        desc = f"""
            === Dosimeter Control Station ===
            Listening on      : {CONTROL_STATION_IP_ADDR}:{RECV_FROM_DOSIMETER_PORT} (TCP)
            Dosimeter IP      : {DOSIMETER_DEFAULT_IP}
            Command port      : {SEND_TO_DOSIMETER_PORT} (TCP, we connect)
            Log file          : {self.log_file}
            Initial period    : {self.period} s
            Channels          : 1-6
            === End Description ===
        """
        print(desc)

    def _init_csv(self) -> None:
        """Create the CSV file with a descriptive header if it does not exist."""
        header = ("timestamp,channel1,channel2,channel3,channel4,channel5,channel6\n"
                  "# YYYY-MM-DD HH:MM:SS,counts,counts,counts,counts,counts,counts\n")
        try:
            with open(self.log_file, 'x', newline='') as f:
                f.write(header)
        except FileExistsError:
            pass  # file already there – assume header exists
  
    def _server_loop(self) -> None:
        """Accept the dosimeter's connection and read telemetry from it."""
        while self.running:
            try:
                conn, addr = self.listen_sock.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            print(f"[Dosimeter] Connected from {addr[0]}")
            self.client_sock = conn
            try:
                self._read_telemetry(conn)
            except OSError as e:
                if self.running:
                    print(f"[Dosimeter] Link to {addr[0]} failed: {e}")
            finally:
                close_quietly(conn)
                self.client_sock = None
                if self.running:
                    print(f"[Dosimeter] Disconnected from {addr[0]}")

    def _read_telemetry(self, conn) -> None:
        """Read newline-delimited CSV lines; the firmware terminates every one."""
        enable_keepalive(conn)
        conn.settimeout(RECV_TIMEOUT)
        buffer = ""

        while self.running:
            try:
                data = conn.recv(4096)
            except socket.timeout:
                # A quiet dosimeter is not an error; keepalive decides.
                continue
            if not data:
                break

            buffer += data.decode("utf-8", errors="ignore")
            while "\n" in buffer:
                line, buffer = buffer.split("\n", 1)
                self._handle_line(line.strip())

            # A peer that never sends a newline must not grow this forever.
            if len(buffer) > 4096:
                print("[Dosimeter] Dropping oversized unterminated line")
                buffer = ""

    def _handle_line(self, line: str) -> None:
        if not line:
            return
        try:
            parsed = self.parse_csv_string(line)
        except (ValueError, IndexError, KeyError) as e:
            # One bad line must not tear down the link.
            print(f"[Dosimeter] Skipping malformed line {line!r}: {e}")
            return

        ts = (f"{parsed['year']:04d}-{parsed['month']:02d}-{parsed['day']:02d} "
              f"{parsed['hour']:02d}:{parsed['minute']:02d}:{parsed['second']:02d}")
        counts = [parsed[f'channel{i}'] for i in range(1, 7)]

        with self.counts_lock:
            self.latest_counts = counts[:]

        if self.log_file_handle:
            with self.logging_lock:
                if self.log_file_handle and not self.log_file_handle.closed:
                    self.log_file_handle.write(f"{ts}," + ",".join(map(str, counts)) + "\n")
                    self.log_file_handle.flush()

        print(f"[{ts}] {', '.join(map(str, counts))}")

    # ------------------------------------------------------------------
    # Parsing & packet creation (unchanged from your stub, minor doc fixes)
    # ------------------------------------------------------------------
    def parse_csv_string(self, csv_string: str) -> Dict[str, int]:
        """Convert the dosimeter CSV line into a dictionary."""
        parts = csv_string.rstrip('\n').split(',')
        date_part, time_part = parts[0].split(' ')
        year, month, day = map(int, date_part.split('-'))
        hour, minute, second = map(int, time_part.split(':'))
        return {
            'year': year, 'month': month, 'day': day,
            'hour': hour, 'minute': minute, 'second': second,
            'channel1': int(parts[1]), 'channel2': int(parts[2]),
            'channel3': int(parts[3]), 'channel4': int(parts[4]),
            'channel5': int(parts[5]), 'channel6': int(parts[6]),
        }

    def create_command_packet(self, command: int, data: int) -> bytes:
        """8-bit command + 24-bit data → big-endian 4-byte packet."""
        if not (0 <= command <= 0xFF):
            raise ValueError("Command must be 0-255")
        if not (0 <= data <= 0xFFFFFF):
            raise ValueError("Data must be 0-16777215")
        value = (command << 24) | (data & 0xFFFFFF)
        return struct.pack('>I', value)

    def create_date_command(self, year: int, month: int, day: int) -> bytes:
        """CMD_SET_DATE | Year|Month|Day → 4-byte packet.

        Without the opcode a date is indistinguishable from a time.
        """
        if not (0 <= year <= 0xFF):
            raise ValueError("Year 0-255")
        if not (0 <= month <= 0xFF):
            raise ValueError("Month 0-255")
        if not (0 <= day <= 0xFF):
            raise ValueError("Day 0-255")
        value = (CMD_SET_DATE << 24) | (year << 16) | (month << 8) | day
        return struct.pack('>I', value)

    def create_time_command(self, hour: int, minute: int, second: int) -> bytes:
        """CMD_SET_TIME | Hour|Minute|Second → 4-byte packet."""
        if not (0 <= hour <= 23):
            raise ValueError("Hour 0-23")
        if not (0 <= minute <= 59):
            raise ValueError("Minute 0-59")
        if not (0 <= second <= 59):
            raise ValueError("Second 0-59")
        value = (CMD_SET_TIME << 24) | (hour << 16) | (minute << 8) | second
        return struct.pack('>I', value)


# ----------------------------------------------------------------------
# Example usage (run this file directly)
# ----------------------------------------------------------------------
if __name__ == "__main__":
    dos = Dosimeter(log_file="dosimeter_log.csv", period_seconds=1)

    # Start listening for data
    dos.start_server()

    # Example: change period to 5 seconds after 3 s
    time.sleep(3)
    dos.blm_send_new_period(5)

    # Example: set date/time (use current UTC for demo)
    now = datetime.utcnow()
    dos.set_date(now.year % 100, now.month, now.day)   # firmware expects 0-99 for year?
    dos.set_time(now.hour, now.minute, now.second)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
        dos.stop_server()