import struct
import socket
import threading
import time
from datetime import datetime
from typing import Dict, Optional

# ----------------------------------------------------------------------
# Configuration constants
# ----------------------------------------------------------------------
SEND_TO_DOSIMETER_PORT = 1972      # Commands → dosimeter
RECV_FROM_DOSIMETER_PORT = 1973    # Data ← dosimeter
CONTROL_STATION_IP_ADDR = "192.168.7.001"   # this PC
TEST_IP_ADDR = "127.0.0.1"
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
    * UDP server that receives CSV lines (timestamp + 6 counts)
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

        # UDP sockets
        self.cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recv_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recv_sock.bind((TEST_IP_ADDR, RECV_FROM_DOSIMETER_PORT))
        self.recv_sock.settimeout(1.0)   # non-blocking receive loop

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

    def start_server(self) -> None:
        """Start the UDP listener in a background thread."""
        if self.running:
            print("[Dosimeter] Server already running.")
            return

        self.running = True
        self.server_thread = threading.Thread(target=self._server_loop, daemon=True)
        self.server_thread.start()
        print(f"[Dosimeter] server listening on {CONTROL_STATION_IP_ADDR}:{RECV_FROM_DOSIMETER_PORT}")

    def stop_server(self) -> None:
        """Stop the UDP listener."""
        self.running = False
        if self.server_thread:
            self.server_thread.join(timeout=2.0)
        self.recv_sock.close()
        self.cmd_sock.close()
        print("[Dosimeter] Server stopped.")

    def blm_send_new_period(self, new_period: int) -> None:
        """
        Change the integration period on the dosimeter.

        Args:
            new_period: Integration time in seconds (24-bit unsigned).
        """
        if not (0 <= new_period <= 0xFFFFFF):
            raise ValueError("Period must be 0–16777215 seconds")
        packet = self.create_command_packet(CMD_SET_PERIOD, new_period)
        self.cmd_sock.sendto(packet, (DOSIMETER_DEFAULT_IP, SEND_TO_DOSIMETER_PORT))
        self.period = new_period
        print(f"[Dosimeter] Sent new period = {new_period} s")

    def set_date(self, year: int, month: int, day: int) -> None:
        """Send a DATE command (8-bit fields)."""
        packet = self.create_date_command(year, month, day)
        self.cmd_sock.sendto(packet, (DOSIMETER_DEFAULT_IP, SEND_TO_DOSIMETER_PORT))
        print(f"[Dosimeter] Sent DATE {year:02d}-{month:02d}-{day:02d}")

    def set_time(self, hour: int, minute: int, second: int) -> None:
        """Send a TIME command (8-bit fields)."""
        packet = self.create_time_command(hour, minute, second)
        self.cmd_sock.sendto(packet, (DOSIMETER_DEFAULT_IP, SEND_TO_DOSIMETER_PORT))
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
            IP (this PC)      : {CONTROL_STATION_IP_ADDR}
            Dosimeter IP      : {DOSIMETER_DEFAULT_IP}
            Recv port         : {RECV_FROM_DOSIMETER_PORT}
            Send port         : {SEND_TO_DOSIMETER_PORT}
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
        while self.running:
            try:
                data, _ = self.recv_sock.recvfrom(4096)
            except socket.timeout:
                print("[Dosimeter]: Socket time out")
                continue
            except OSError:
                break

            csv_line = data.decode('utf-8', errors='ignore')
            parsed = self.parse_csv_string(csv_line)

            # --- Build ISO timestamp ---
            ts = f"{parsed['year']:04d}-{parsed['month']:02d}-{parsed['day']:02d} " \
                    f"{parsed['hour']:02d}:{parsed['minute']:02d}:{parsed['second']:02d}"

            # --- Update UI counts ---
            counts = [parsed[f'channel{i}'] for i in range(1, 7)]
            with self.counts_lock:
                self.latest_counts = counts[:]

            # --- LOGGING (only if enabled) ---
            if self.log_file_handle:
                with self.logging_lock:
                    if self.log_file_handle and not self.log_file_handle.closed:
                        # Use the *current* channel names for ordering (defensive)
                        data_line = f"{ts}," + ",".join(map(str, counts)) + "\n"
                        self.log_file_handle.write(data_line)
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
        """Year|Month|Day → 4-byte packet (upper 8 bits zero)."""
        if not (0 <= year <= 0xFF):
            raise ValueError("Year 0-255")
        if not (0 <= month <= 0xFF):
            raise ValueError("Month 0-255")
        if not (0 <= day <= 0xFF):
            raise ValueError("Day 0-255")
        value = (year << 16) | (month << 8) | day
        return struct.pack('>I', value)

    def create_time_command(self, hour: int, minute: int, second: int) -> bytes:
        """Hour|Minute|Second → 4-byte packet (upper 8 bits zero)."""
        if not (0 <= hour <= 23):
            raise ValueError("Hour 0-23")
        if not (0 <= minute <= 59):
            raise ValueError("Minute 0-59")
        if not (0 <= second <= 59):
            raise ValueError("Second 0-59")
        value = (hour << 16) | (minute << 8) | second
        return struct.pack('>I', value)

