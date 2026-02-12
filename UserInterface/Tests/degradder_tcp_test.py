import socket
# Acting a the degrader to test command recieve
HOST = "127.0.0.1"  # The server's hostname or IP address
PORT = 1971        # The port used by the server

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
    client_socket.connect((HOST, PORT))
    client_socket.sendall(b"123")
    data = client_socket.recv(1024)

print("Received from server:", data.decode())