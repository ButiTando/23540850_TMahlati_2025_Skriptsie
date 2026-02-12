import cv2
import threading
import requests
import time
import tkinter as tk
import numpy as np
from PIL import Image, ImageTk

Test_mode = True

class Test_Camera:
    CAM_TAG = {
        "stream_load_error": "CAM_NO_CONN",
        "rtsp_connection_successful": "CAM_OK",
        "rtsp_connection_closed": "CAM_CLOSE",
    }

    def __init__(self, parent,camera_ip):
        self.rtsp_url = None
        self.jpeg_url = f'http://{camera_ip}/snap.jpeg'
        self.camera_ip = camera_ip
        self.cap = None
        self.latest_frame = None
        self.lock = threading.Lock()
        self.running = True
        self.thread = None
        self.capture_thread = None
        self.connected_widget = None
        self.is_connected = False
        parent.pack_propagate(False)
        self.parent = parent

        # Label to display frames
        self.video_label = tk.Label(parent, text = "Camera not setup")
        self.video_label.pack(expand=True, fill="both")        

        # Track frame size for scaling
        self.frame_width = 1
        self.frame_height = 1
        self.video_label.bind("<Configure>", self.on_resize)
        self.parent.bind("<Destroy>", lambda e: self.stop())

    def capture(self):
        """
        Initiating OpenCV capture and starting capture thread in daemon mode.
        """
        if (Test_mode == False):
            try:
                self.cap = cv2.VideoCapture(self.rtsp_url)

                if not self.cap.isOpened():
                    self.running = False
                    self.video_label.configure(text=f'Failed to get camera feed at {self.camera_ip}\nPlease confirm IP address')
                      

                else:
                    self.running = True
                    self.is_connected = True
                    print("Starting grab frame thread")
                    self.thread = threading.Thread(target=self._grab_frames, daemon=True)
                    self.thread.start()

                    # Start periodic capture of frame.
                    self.update_frame()
                    self.connected_widget.configure(text="Connected")

            except Exception as e:
                self.running = False                
                self.camera_dummy_view.configure(text=f'Failed to get camera feed at {self.camera_ip}\nPlease confirm IP address')

        else:
            self.cap = cv2.VideoCapture(0)
            self.running = True
            print("Starting grab frame thread")
            self.thread = threading.Thread(target=self._grab_frames, daemon=True)
            self.thread.start()

            # Start periodic capture of frame.
            self.update_frame()
            self.connected_widget.configure(text="Connected")
        
        # Check if stream connection successful
        if not self.cap.isOpened():
            # raise Exception(self.CAM_TAG["steam_load_error"])
            return print(f'{self.CAM_TAG["stream_load_error"]}: Failed to connect to {self.jpeg_url}')
        
    def _grab_frames(self):
        # continuously grab frames
        while self.running:
            try:
                if(Test_mode == False):
                    # Fetch the JPEG image
                    response = requests.get(self.jpeg_url, timeout=5)
                    if response.status_code == 200:
                        # Convert JPEG data to NumPy array
                        img_array = np.frombuffer(response.content, dtype=np.uint8)
                        # Decode to OpenCV frame (BGR format)
                        frame = cv2.imdecode(img_array, cv2.IMREAD_COLOR)
                        
                        if frame is not None:
                            with self.lock:
                                self.latest_frame = frame
                        else:
                            print("Failed to decode frame")
                    else:
                        print(f"HTTP Error: {response.status_code}")

                else:
                    ret, frame =  self.cap.read()
                    # Convert JPEG data to NumPy array                  
                    
                    if ret:
                        with self.lock:
                            self.latest_frame = frame
                    else:
                        print("Failed to decode frame")

            except requests.RequestException as e:
                print(f"Error fetching frame: {e}")
            except cv2.error as e:
                print(f"OpenCV error: {e}")
            # Limit requests to 60fps
            time.sleep(0.0167)
           
    def on_resize(self, event):
        """Update stored frame size when the widget is resized."""
        self.frame_width = event.width
        self.frame_height = event.height

    def update_frame(self):
        # Get current size of the label (or default if not drawn yet)
        frame_width = self.video_label.winfo_width()
        frame_height = self.video_label.winfo_height()
        with self.lock:
            if self.latest_frame is not None:
                frame = self.latest_frame

                # Convert from BGR (OpenCV) to RGB (PIL)
                frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                frame = cv2.flip(frame, 1)  # Flip horizontally
                img = Image.fromarray(frame)

                # Keep aspect ratio
                img_ratio = img.width / img.height
                frame_ratio = frame_width / frame_height

                if img_ratio > frame_ratio:
                    new_width = frame_width
                    new_height = int(frame_width / img_ratio)
                else:
                    new_height = frame_height
                    new_width = int(frame_height * img_ratio)

                # Ensure width & height are at least 1
                new_width = max(1, new_width)
                new_height = max(1, new_height)

                img = img.resize((new_width, new_height), Image.LANCZOS)
                imgtk = ImageTk.PhotoImage(image=img)

                self.video_label.imgtk = imgtk
                self.video_label.configure(image=imgtk)
                
        # Schedule next update (16ms ~ 60 FPS)
        self.parent.after(16, self.update_frame)

    # Stopping feed and killing thread.
    def stop(self):
        self.running = False

        if self.thread:
            self.thread.join()

        if self.cap:
            self.cap.release()

        if self.capture_thread:
            self.capture_thread.join()

        return f'{self.CAM_TAG["jpeg_connection_closed"]}: Camera feed closed.'
            


