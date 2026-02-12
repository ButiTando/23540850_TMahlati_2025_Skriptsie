from enum import Enum
from threading import Thread, Lock
import socket

# live feedback states
lens_state = {
    0: "off",
    1: "on",
    2: "updating"
}

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

# Degrader object
class Degrader():

    def __init__(self, testing=True): #toggle box swithc
        self.degrader_param = {
            "command": 0,
            "response": 0, 
            "connection": Connection_state.DISCONNECTED.value["text"]           
        }
        if testing == True:        
            self.my_ip_addr = "127.0.0.1"
            self.degradder_ip = socket.gethostbyname(socket.gethostname())
            
        elif testing == False:
             self.my_ip_addr = "192.168.007.001"
             self.degradder_ip = None
             
        self.server_socket = None
        self.is_server_running = None
        self.degrader_response = None
        self.degrader_frame = None
        self.resp = Response()
        self.desired_state = Command()
        self.request_connect = True
        server_thread = Thread(target=self.start_server, daemon=True)
        server_thread.start()
        self.degrader_conn_status_frame = None

        if server_thread is None or not server_thread.is_alive():
            self.is_server_running = False

        self.response_lock = Lock()
        
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
        
        message = f'{self.desired_state.command}'
        print(f'sending: {message}')
        send_command_thread = Thread(target=self._sendData, args=(self.degradder_ip, SEND_TO_DEGRADER_PORT, message), daemon=True)
        send_command_thread.start()

    def _sendData(self, dest_ip, dest_port, message):

        # Create a TCP socket

        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
            try:
                # Connect to the server
                    client_socket.connect((dest_ip, dest_port))
                    
                    # Send the message
                    client_socket.sendall(message.encode('utf-8'))
            
            except Exception as e:
                print(f'Error: {e}')


        # client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # try:
        #     # Connect to the server
        #     client_socket.connect((dest_ip, dest_port))
            
        #     # Send the message
        #     client_socket.sendall(message.encode('utf-8'))
            
        # except Exception as e:
        #     print(f"Error: {e}")
            
        # finally:
        #     # Close the socket
        #     client_socket.close()

    def start_server(self):
        # Create server socket
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Try to bind the socket to IP and port then listen.
        try:

            # if(current_ip != CONTROL_STATION_IP_ADDR):
            #     CONTROL_STATION_IP_ADDR = current_ip
            #     self.testing_mode = True
            #     print("Please configure IP correctly")
            
            self.server_socket.bind((self.my_ip_addr, RECV_FROM_DEGRADER_PORT))
            self.server_socket.listen((5)) #Listen to one client at max
            print(f'Degrader server started: ip->{self.my_ip_addr} port->{RECV_FROM_DEGRADER_PORT}')

            # Polling for connection
            while True:
                # Accept client connection
                client_socket, addr = self.server_socket.accept()
                
                # Get data from the client 
                try:
                    while True:
                        data = client_socket.recv(1024)
                        if not data:
                            # No more data to recv break
                            break
                        else:
                            with self.response_lock:
                                self.degrader_response = data.decode('utf-8')
                                print(self.degrader_response)
                            self.decode_response(self.degrader_response)

                except Exception as e:
                    print(f'Error with client {addr}: {e}')

                finally:
                    client_socket.close()
        

        except Exception as e:
            print(f'Error: {Connection_state.SERVER_BIND_ERROR.value["text"]}\n {e}')

        finally:
            # Gracefully close connect.
            print("Degrader close unexpectedly")
            self.server_socket.close()

    def set_command(self, lens_values):

        self.desired_state.lens_2mm = lens_values["2mm"]["desired_state"]
        self.desired_state.lens_3mm = lens_values["3mm"]["desired_state"]
        self.desired_state.lens_6mm = lens_values["6mm"]["desired_state"]
        self.desired_state.lens_8mm = lens_values["8mm"]["desired_state"]
        self.desired_state.lens_10mm = lens_values["10mm"]["desired_state"]
        self.desired_state.lens_12mm = lens_values["12mm"]["desired_state"]
        self.desired_state.lens_30mm = lens_values["30mm"]["desired_state"]

    def decode_response(self, response):
        with self.response_lock:
            self.resp.response = int(self.degrader_response)
            print(f'Lens 2mm : {lens_state[self.resp.lens_status_2mm]} | Lens 3mm : {lens_state[self.resp.lens_status_3mm]} | Lens 6mm : {lens_state[self.resp.lens_status_6mm]} | Lens 8mm : {lens_state[self.resp.lens_status_8mm]} | Lens 10mm : {lens_state[self.resp.lens_status_10mm]} | Lens 12mm : {lens_state[self.resp.lens_status_12mm]} | Lens 30mm : {lens_state[self.resp.lens_status_30mm]} | State : {self.resp.process_status}')

            if((self.resp.process_status == 1) and self.request_connect == True):
                self.degrader_conn_status_frame.configure(text="Connected")
                self.request_connect = False
                self.updating
            
    def degrader_close(self):
        self.server_socket.close()

    def degrader_response_state_machine(self):
        pass
