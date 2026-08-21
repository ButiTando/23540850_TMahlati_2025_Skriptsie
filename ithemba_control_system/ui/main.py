import tkinter as tk
import cv2
from PIL import Image, ImageTk


# Custom module for project
from radiation_systems import Platform, Degrader, Dosimeter, Test_Camera


class CameraWidget:

    '''
    The stream_url used is from the UniFi video page loaded when the camera's IP is typed in a browser.
    Assuming that open DHCP is set to assign a static IP (192.168.7.102) to the camera.
    If the camera is assigned a dynamic IP, then camera_ip should be set to that IP.
    Username = ubnt
    password = ubnt

    form1.cs search -> http://
    '''
    def __init__(self, parent, stream_url = "rtsp://192.168.7.102:554/s0", camera_ip:str = None):   

        camera_selected = None
        # Choose where to get the video stream.
        if(camera_ip == None):
            camera_selected = 0
        
        else:
            camera_selected = "rtsp://{}:554/s0".format(camera_ip)

        parent.pack_propagate(False)
        self.parent = parent

        # Label to display video
        self.video_lable = tk.Label(parent)
        self.video_lable.pack(expand=True, fill="both")

        # Starting capture at IP address.
        self.cam = Test_Camera(camera_selected)
        self.feed_back = self.cam.caputure()        

        # Track frame size for scaling
        self.frame_width = 1
        self.frame_height = 1
        self.video_lable.bind("<Configure>", self.on_resize)

        self.parent.bind("<Destroy>", lambda e: self.on_close())

    def on_resize(self, event):
        """Update stored frame size when the widget is resized."""
        self.frame_width = event.width
        self.frame_height = event.height

    def update_frame(self):
        # Get current size of the label (or default if not drawn yet)
        frame_width = self.video_lable.winfo_width() or 320
        frame_height = self.video_lable.winfo_height() or 240

        frame = self.cam.get_latest_frame()

        if frame != None:

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

            self.video_lable.imgtk = imgtk
            self.video_lable.configure(image=imgtk)

        else:
            pass

        self.parent.after(33, self.update_frame)  # ~60 FPS

    def on_close(self):
        self.cam.stop()

class SquareWidgetApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Ithemba Control station app")
        self.root.geometry("800x600")

        self.widgets_info = [
            {"name": "CAM_STREAM", "color": "#02053d", "details": "Camera feed here"},
            {"name": "Degrader", "color": "#02053d", "details": "Degradder info"},
            {"name": "Sensor Platform", "color": "#02053d", "details": "platform setting"},
            {"name": "Results", "color": "#02053d", "details": "Sensor reading"}
        ]

        self.main_frame = tk.Frame(self.root)
        self.main_frame.pack(expand=True, fill="both", padx=20, pady=20)       

        # Equal grid sizing
        for row in range(2):
            self.main_frame.grid_rowconfigure(row, weight=1, minsize=250)
        for col in range(2):
            self.main_frame.grid_columnconfigure(col, weight=1, minsize=250)

        for i, info in enumerate(self.widgets_info):
            # Main widget frame
            frame = tk.Frame(self.main_frame, bg=info["color"], bd=2, relief="ridge")
            frame.grid(row=i//2, column=i%2, padx=10, pady=10, sticky="nsew")

            # Title bar frame (small buttons at the top)
            title_bar = tk.Frame(frame, bg="#111144", height=30)
            title_bar.pack(fill="x")

            # Small buttons on the title bar
            btn_expend = tk.Button(title_bar, text="_", bg="yellow", fg="black", width=2, command=lambda idx=i: self.open_info_window(idx))
            btn_expend.pack(side="right", padx=2, pady=2)

            # Title label
            title_label = tk.Label(title_bar, text=info["name"], bg="#111144", fg="white", font=("Arial", 12, "bold"))
            title_label.pack(side="left", padx=5)

            # Content frame
            content_frame = tk.Frame(frame, bg=info["color"])
            content_frame.pack(expand=True, fill="both")

            if info["name"] == "CAM_STREAM":
                CameraWidget(content_frame)  # Assuming CameraFeed takes a frame
            else:
                label = tk.Label(content_frame, text=info["details"], fg="white", bg=info["color"], font=("Arial", 14))
                label.pack(expand=True, fill="both")

    def open_info_window(self, idx):
        print(f"Open info window for {self.widgets_info[idx]['name']}")

    def close_widget(self, idx):
        print(f"Close widget {self.widgets_info[idx]['name']}")

    def minimize_widget(self, idx):
        print(f"Minimize widget {self.widgets_info[idx]['name']}")

    def open_info_window(self, index):
        info = self.widgets_info[index]
        info_window = tk.Toplevel(self.root)
        info_window.title(info["name"])
        info_window.geometry("400x300")
        info_window.configure(background="green")

        label = tk.Label(
            info_window,
            text=info["details"],
            font=("Arial", 12),
            justify="left",
            wraplength=350,
            padx=20,
            pady=20
        )
        label.pack(expand=True, fill="both")

        close_button = tk.Button(
            info_window,
            text="Close",
            command=info_window.destroy,
            font=("Arial", 12)
        )
        close_button.pack(pady=10)


if __name__ == "__main__":
    root = tk.Tk()
    app = SquareWidgetApp(root)
    root.mainloop()
