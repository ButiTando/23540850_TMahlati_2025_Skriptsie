import socket
from radiation_systems import Response

HOST = socket.gethostbyname(socket.gethostname())
PORT = 1970
resp = Response(0)
resp.process_status = 2
respo = resp.response
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
    
    server_socket.bind((HOST, PORT))
    server_socket.listen(5)
    while(True):
        print(f"Server listening on {HOST}:{PORT}")
        conn, addr = server_socket.accept()
        with conn:
            print(f"Connected by {addr}")
            data = conn.recv(1024)
            print(f'Received: {data.decode()}')
            conn.sendall(f'{resp.response}'.encode())
            

            