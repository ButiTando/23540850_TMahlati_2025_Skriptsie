from enum import Enum
from threading import Thread, Lock
import re
import socket
import time

from .net_utils import (local_lan_ip, make_tcp_listener, enable_keepalive,
                        close_quietly)
# live feedback states
lens_state = {
    0: "off",
    1: "on",
    2: "updating",
    3: "unchanged"
}


def lens_text(value):
    """Name a 2-bit lens status without ever raising on an unmapped value."""
    return lens_state.get(value, f"unknown({value})")

class Connection_state(Enum):
    CONNECTED = {"text": "Connected", "value":  1}
    DISCONNECTED = {"text": "Disconnected", "value": 2}
    SERVER_BIND_ERROR = {"text": "Failed to create degrader packet receiving server.", "value": 3}

class Command:
    def __init__(self, command: int = 0):
        self._command = command & 0xFF  # Ensure it's an 8-bit value

    @property
    def command(self) -> int:
        return self._command

    @command.setter
    def command(self, value: int):
        self._command = value & 0xFF  # Keep within 8 bits

    # Bit properties for each lens and probe
    @property
    def lens_2mm(self) -> int:
        return (self._command >> 0) & 1

    @lens_2mm.setter
    def lens_2mm(self, value: int):
        self._command = (self._command & ~1) | ((value & 1) << 0)

    @property
    def lens_3mm(self) -> int:
        return (self._command >> 1) & 1

    @lens_3mm.setter
    def lens_3mm(self, value: int):
        self._command = (self._command & ~(1 << 1)) | ((value & 1) << 1)

    @property
    def lens_6mm(self) -> int:
        return (self._command >> 2) & 1

    @lens_6mm.setter
    def lens_6mm(self, value: int):
        self._command = (self._command & ~(1 << 2)) | ((value & 1) << 2)

    @property
    def lens_8mm(self) -> int:
        return (self._command >> 3) & 1

    @lens_8mm.setter
    def lens_8mm(self, value: int):
        self._command = (self._command & ~(1 << 3)) | ((value & 1) << 3)

    @property
    def lens_10mm(self) -> int:
        return (self._command >> 4) & 1

    @lens_10mm.setter
    def lens_10mm(self, value: int):
        self._command = (self._command & ~(1 << 4)) | ((value & 1) << 4)

    @property
    def lens_12mm(self) -> int:
        return (self._command >> 5) & 1

    @lens_12mm.setter
    def lens_12mm(self, value: int):
        self._command = (self._command & ~(1 << 5)) | ((value & 1) << 5)

    @property
    def lens_30mm(self) -> int:
        return (self._command >> 6) & 1

    @lens_30mm.setter
    def lens_30mm(self, value: int):
        self._command = (self._command & ~(1 << 6)) | ((value & 1) << 6)

    @property
    def probe_bit(self) -> int:
        return (self._command >> 7) & 1

    @probe_bit.setter
    def probe_bit(self, value: int):
        self._command = (self._command & ~(1 << 7)) | ((value & 1) << 7)

# Response union emulator
class Response:
    def __init__(self, response=0):
        self.response = response  # 16-bit integer

    def as_dict(self):
        return{
            "2mm": self.lens_status_2mm,
            "3mm": self.lens_status_3mm,
            "6mm": self.lens_status_6mm,
            "8mm": self.lens_status_8mm,
            "10mm": self.lens_status_10mm,
            "12mm": self.lens_status_12mm,
            "30mm": self.lens_status_30mm
        }

    # 2-bit fields
    @property
    def lens_status_2mm(self):
        return (self.response >> 0) & 0b11

    @lens_status_2mm.setter
    def lens_status_2mm(self, value):
        self.response = (self.response & ~(0b11 << 0)) | ((value & 0b11) << 0)

    @property
    def lens_status_3mm(self):
        return (self.response >> 2) & 0b11

    @lens_status_3mm.setter
    def lens_status_3mm(self, value):
        self.response = (self.response & ~(0b11 << 2)) | ((value & 0b11) << 2)

    @property
    def lens_status_6mm(self):
        return (self.response >> 4) & 0b11

    @lens_status_6mm.setter
    def lens_status_6mm(self, value):
        self.response = (self.response & ~(0b11 << 4)) | ((value & 0b11) << 4)

    @property
    def lens_status_8mm(self):
        return (self.response >> 6) & 0b11

    @lens_status_8mm.setter
    def lens_status_8mm(self, value):
        self.response = (self.response & ~(0b11 << 6)) | ((value & 0b11) << 6)

    @property
    def lens_status_10mm(self):
        return (self.response >> 8) & 0b11

    @lens_status_10mm.setter
    def lens_status_10mm(self, value):
        self.response = (self.response & ~(0b11 << 8)) | ((value & 0b11) << 8)

    @property
    def lens_status_12mm(self):
        return (self.response >> 10) & 0b11

    @lens_status_12mm.setter
    def lens_status_12mm(self, value):
        self.response = (self.response & ~(0b11 << 10)) | ((value & 0b11) << 10)

    @property
    def lens_status_30mm(self):
        return (self.response >> 12) & 0b11

    @lens_status_30mm.setter
    def lens_status_30mm(self, value):
        self.response = (self.response & ~(0b11 << 12)) | ((value & 0b11) << 12)

    @property
    def process_status(self):
        return (self.response >> 14) & 0b11

    @process_status.setter
    def process_status(self, value):
        self.response = (self.response & ~(0b11 << 14)) | ((value & 0b11) << 14)

SEND_TO_DEGRADER_PORT = 1970
RECV_FROM_DEGRADER_PORT = 1971
CONTROL_STATION_IP_ADDR = "192.168.007.001"

LISTEN_HOST = "0.0.0.0"    # every interface; the auto-detected one was loopback
CONNECT_TIMEOUT = 3.0
RECV_TIMEOUT = 10.0
REBIND_DELAY = 2.0

# The firmware sends bare ASCII digits, no terminator, every 500 ms.
MAX_RESPONSE_VALUE = 0xFFFF
# Past a tenth of the maximum a token cannot take another digit, so it can be
# emitted immediately - no timer needed.
_MAX_EXTENDABLE = MAX_RESPONSE_VALUE // 10      # 6553

# Gap that ends a small token: above Linux's 200 ms minimum RTO, below the
# 500 ms cadence. An unterminated peer faster than 4 Hz will mis-frame.
IDLE_FLUSH = 0.25
# Silence this long means the peer is gone. The accept loop serves one
# connection at a time, so a half-open socket would block the reconnect.
LINK_SILENCE = 10.0


class ResponseFramer:
    """Recover 16-bit response values from an unframed ASCII digit stream.

    The degrader sends snprintf("%d") output with no terminator, so framing
    relies on two properties of that producer: a value never exceeds 65535, and
    "%d" never pads, so a leading '0' is always the whole value. Together these
    frame most tokens on arrival; only values <= 6553 wait for a gap.

    The accumulator holds at most four digits, so it is bounded by design.
    """

    def __init__(self):
        self._digits = ""

    @property
    def pending(self):
        return bool(self._digits)

    def feed(self, data):
        """Absorb bytes; return the response values that are now complete."""
        values = []
        for char in data:
            if not char.isdigit():
                # Any non-digit ends the token and is discarded.
                values.extend(self.flush())
                continue

            if self._digits == "0":
                # '0' cannot be a prefix, so this digit starts a new value.
                values.append(0)
                self._digits = ""

            if self._digits and int(self._digits) * 10 + int(char) > MAX_RESPONSE_VALUE:
                # Taking this digit would overflow, so the token ended here.
                values.append(int(self._digits))
                self._digits = ""

            self._digits += char

            if int(self._digits) > _MAX_EXTENDABLE:
                # No further digit can follow: emit without waiting.
                values.append(int(self._digits))
                self._digits = ""
        return values

    def flush(self):
        """End the token in hand, if any."""
        if not self._digits:
            return []
        value = int(self._digits)
        self._digits = ""
        return [value]

# Degrader object
class Degrader():

    def __init__(self, testing=True, listen_port=None, command_port=None): #toggle box swithc
        self.degrader_param = {
            "command": 0,
            "response": 0, 
            "connection": Connection_state.DISCONNECTED.value["text"]           
        }
        if testing == True:
            self.my_ip_addr = local_lan_ip()
            self.degradder_ip = self.my_ip_addr

        elif testing == False:
             self.my_ip_addr = "192.168.007.001"
             self.degradder_ip = None

        # Overridable so tests need not compete for 1970/1971.
        self.listen_port = listen_port or RECV_FROM_DEGRADER_PORT
        self.command_port = command_port or SEND_TO_DEGRADER_PORT

        self.server_socket = None
        self.client_socket = None
        self.is_server_running = None
        self.degrader_response = None
        self.degrader_frame = None
        self.resp = Response()
        self.desired_state = Command()
        self.request_connect = True
        self.degrader_conn_status_frame = None

        # Must exist before the thread starts, or it dies on the first packet.
        self.response_lock = Lock()
        self._running = True

        self.server_thread = Thread(target=self.start_server, daemon=True)
        self.server_thread.start()

        if self.server_thread is None or not self.server_thread.is_alive():
            self.is_server_running = False

    def start_degrader(self, host_ip_addr, degrader_ip_addr):
        """
        Starts server that listens for degrader messages(response) and sends are you degrader command to 

        Args:
            ip_addr (str): ip address of the host machine in the networt.
            degrader_ip_add (str): ip address of the degrader on the network.

        returns:
            None
        """
        
        self.degradder_ip = degrader_ip_addr   
        self.desired_state.probe_bit = 1
        self.send_command()
        self.request_connect = True
        self.desired_state.probe_bit = 0

    def send_command(self):
        """
        Sends message to degrader via a TCP socket.
        Must be called only after start_degrader.

        Args:
            command (Degrader_command): degrader command.

        returns:
            None
        """
        
        # Always three digits: the firmware memcpy's exactly 3 and never
        # null-terminates, so a shorter command leaves atoi() reading junk.
        message = f'{self.desired_state.command:03d}'
        print(f'sending: {message}')
        send_command_thread = Thread(target=self._sendData, args=(self.degradder_ip, self.command_port, message), daemon=True)
        send_command_thread.start()

    def _sendData(self, dest_ip, dest_port, message):

        if not dest_ip:
            print("Error: no degrader IP set - press Connect first")
            return

        # Create a TCP socket
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            try:
                    # Without this a missing degrader blocks for the OS
                    # default, over a minute on Windows.
                    client_socket.settimeout(CONNECT_TIMEOUT)

                    # Connect to the server
                    client_socket.connect((dest_ip, dest_port))

                    # Newline-terminated so two commands in one segment split.
                    client_socket.sendall((message + '\n').encode('utf-8'))

            except Exception as e:
                print(f'Error: {e}')

    def start_server(self):
        """Accept degrader connections forever, surviving individual failures."""
        while self._running:
            try:
                self.server_socket = make_tcp_listener(LISTEN_HOST, self.listen_port)
            except OSError as e:
                # A previous run may still hold the port; keep retrying.
                print(f'Error: {Connection_state.SERVER_BIND_ERROR.value["text"]}\n {e}')
                if not self._running:
                    break
                time.sleep(REBIND_DELAY)
                continue

            self.is_server_running = True
            print(f'Degrader server started: ip->{LISTEN_HOST} port->{self.listen_port}')

            try:
                self._accept_loop()
            except OSError as e:
                if self._running:
                    print(f'Degrader server error, restarting: {e}')
            finally:
                close_quietly(self.server_socket)
                self.server_socket = None
                self.is_server_running = False

            if self._running:
                time.sleep(REBIND_DELAY)

        print("Degrader server stopped")

    def _accept_loop(self):
        """Serve one degrader connection at a time until shutdown."""
        self.server_socket.settimeout(1.0)
        while self._running:
            try:
                client_socket, addr = self.server_socket.accept()
            except socket.timeout:
                # Periodic wake-up so shutdown is noticed promptly.
                continue

            self.client_socket = client_socket
            try:
                self._serve_client(client_socket, addr)
            except Exception as e:
                # Errors during shutdown are expected: closing the socket is
                # how this thread is woken out of recv().
                if self._running:
                    print(f'Error with client {addr}: {e}')
            finally:
                close_quietly(client_socket)
                self.client_socket = None

    def _serve_client(self, client_socket, addr):
        """Read responses from one degrader connection, framed or not."""
        enable_keepalive(client_socket)
        client_socket.settimeout(IDLE_FLUSH)
        framer = ResponseFramer()
        last_data = time.monotonic()

        while self._running:
            try:
                data = client_socket.recv(1024)
            except socket.timeout:
                # A gap in the stream completes any small token being held.
                for value in framer.flush():
                    self._safe_handle(value)
                if time.monotonic() - last_data > LINK_SILENCE:
                    print(f'Degrader {addr} silent for {LINK_SILENCE:.0f}s, dropping')
                    break
                continue

            if not data:
                # Peer closed: emit the residue rather than discarding it.
                for value in framer.flush():
                    self._safe_handle(value)
                break

            last_data = time.monotonic()
            for value in framer.feed(data.decode('utf-8', errors='ignore')):
                self._safe_handle(value)

    def _safe_handle(self, value):
        """Decode one response, containing any error to that response."""
        try:
            self._handle_response(value)
        except Exception as e:
            print(f'Error decoding degrader response {value!r}: {e}')

    def _handle_response(self, value):
        # Not a truthiness test: 0 is a valid response.
        if value is None:
            return
        with self.response_lock:
            self.degrader_response = str(value)
            print(self.degrader_response)
        self.decode_response(value)

    def set_command(self, lens_values):

        self.desired_state.lens_2mm = lens_values["2mm"]["desired_state"]
        self.desired_state.lens_3mm = lens_values["3mm"]["desired_state"]
        self.desired_state.lens_6mm = lens_values["6mm"]["desired_state"]
        self.desired_state.lens_8mm = lens_values["8mm"]["desired_state"]
        self.desired_state.lens_10mm = lens_values["10mm"]["desired_state"]
        self.desired_state.lens_12mm = lens_values["12mm"]["desired_state"]
        self.desired_state.lens_30mm = lens_values["30mm"]["desired_state"]

    def decode_response(self, response):
        try:
            value = int(response)
        except (TypeError, ValueError):
            # Garbage must not tear down a working connection.
            print(f'Ignoring malformed degrader response: {response!r}')
            return

        with self.response_lock:
            self.resp.response = value
            print(f'Lens 2mm : {lens_text(self.resp.lens_status_2mm)} | Lens 3mm : {lens_text(self.resp.lens_status_3mm)} | Lens 6mm : {lens_text(self.resp.lens_status_6mm)} | Lens 8mm : {lens_text(self.resp.lens_status_8mm)} | Lens 10mm : {lens_text(self.resp.lens_status_10mm)} | Lens 12mm : {lens_text(self.resp.lens_status_12mm)} | Lens 30mm : {lens_text(self.resp.lens_status_30mm)} | State : {self.resp.process_status}')

            # 1 = awake, 3 = ready. The firmware overwrites awake before it
            # reaches the wire, so ready has to count too.
            connected = (self.resp.process_status in (1, 3)) and self.request_connect == True
            if connected:
                self.request_connect = False

        if connected:
            self._set_connection_text("Connected")

    def _set_connection_text(self, text):
        """Update the Tk label from the main thread; Tk is not thread-safe."""
        frame = self.degrader_conn_status_frame
        if frame is None:
            return
        try:
            frame.after(0, lambda: frame.configure(text=text))
        except Exception:
            pass

    def degrader_close(self):
        """Stop the server thread and release the port."""
        self._running = False
        # Closing the sockets wakes the thread out of accept()/recv().
        close_quietly(self.client_socket)
        close_quietly(self.server_socket)
        thread = getattr(self, "server_thread", None)
        if thread is not None and thread.is_alive():
            thread.join(timeout=2.0)

    def degrader_response_state_machine(self):
        pass
