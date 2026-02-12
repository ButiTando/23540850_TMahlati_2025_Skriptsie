import socket 
import time
from radiation_systems import Response


control_station_ip = socket.gethostbyname(socket.gethostname())
control_station_listen_port = 1971

degrader_ip = socket.gethostbyname(socket.gethostname())
degrader_listen_port = 1970

lens_state = 0 # Assume this is a uint 8 number.
receive_command = 0
command = 0
new_command = False
response = 0 #This is a 16 bit number, with each bit pair representing the state of a lens, and the last bit pair representing wether the command has executed.
current_lens_state = 0 #This is the previous state before a new state is set.
number_of_lenses = 7

response = Response(0)
response.lens_status_6mm = 1
response.lens_status_2mm = 0
response.lens_status_10mm = 0
response.lens_status_3mm = 0
response.lens_status_12mm = 0
response.lens_status_8mm = 0
response.lens_status_30mm = 0

response.process_status = 0

def main():
    
    # Create a TCP socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect to the server
        client_socket.connect((control_station_ip, control_station_listen_port))
        print(f"Connected to server {control_station_ip}:{control_station_listen_port}")

        # Send a message to the server
        message = f'{response.response}'
        client_socket.sendall(message.encode())
        print(f"Sent: {message}")


    except ConnectionRefusedError:
        print("Could not connect to the server — check IP, port, or if the server is running.")
    finally:
        # Close the connection
        client_socket.close()
        print("Connection closed.")

if __name__ == "__main__":
    main()