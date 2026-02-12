import tkinter as tk
from radiation_systems import *
from enum import Enum
import time
from datetime import datetime
import threading

class System_id(Enum):
    Camera_id = 0
    Degrader_id = 1
    XY_Platform_id = 2
    BLM_id = 3

# Variables 
button_state = {
    "off": {"value": 0, "text": "Off beam", "color":"green"},
    "on": {"value": 1, "text": "In beam", "color": "red"},
    "to_off": {"value": 2, "text": "In -> Off", "color": "blue"},
    "to_on": {"value": 3,"text": "Off -> on", "color": "blue"},
    "updating": {"value": 4, "text": "Moving", "color": "orange"},
}

lens_state = {
    0: "off",
    1: "on",
    2: "updating"
}


class MainWidget:
    def __init__(self, root):

        # Create system objects
        self.sub_system_degrader: Degrader = Degrader()
        self.sub_system_xy_platform: Platform = Platform()
        self.sub_system_blm: Dosimeter = Dosimeter()
        

        # Variables
        # Channel labels and values
        self.channel_names = [f"Channel {i+1}" for i in range(6)]  # Default names
        self.channel_values = [0] * 6 
        self.lens_btn_state = {"2mm": {"current":"off", "previous": "off", "desired_state": 0},
                               "3mm":  {"current":"off", "previous": "off", "desired_state": 0},
                               "6mm":  {"current":"off", "previous": "off", "desired_state": 0},
                               "8mm":  {"current":"off", "previous": "off", "desired_state": 0},
                               "10mm":  {"current":"off", "previous": "off", "desired_state": 0},
                               "12mm":  {"current":"off", "previous": "off", "desired_state": 0},
                               "30mm":  {"current":"off", "previous": "off", "desired_state": 0}
                               }
        
        self.updating_degrader = False

        self.channel_names_lock = threading.Lock()

        # Global layout 
        self.blm_entry = None
        

        #  App layout
        self.root = root
        self.root.title("Itemba control station app")
        self.root.geometry("900x600")
        self.root.protocol("WM_DELETE_WINDOW", self.on_app_close)

        self.main_frame = tk.Frame(self.root)
        self.main_frame.pack(expand=True, fill="both")

        self.camera_feed_available = True
        self.cam = None

        # Uni-code character applied to the expand buttons.
        self.square_character = "\u25A0"

        # Set main grid layout 3x3  (rowxcol) grid
        # Configure the rows 
        for row in range(3):
            self.main_frame.grid_rowconfigure(row, weight=1, minsize=200)

        # Configure the column
        for col in range(3):
            self.main_frame.grid_columnconfigure(col, weight=1, minsize=200)

        self.add_camera_widget()
        self.add_degrader_control_widget()
        self.add_xy_platform_control_widget()
        self.add_dlm_control_widget()        

    def add_camera_widget(self):
        # Place widgets on the main frame.
        # Camera widget
        camera_frame = tk.Frame(self.main_frame, bd=1, relief="solid")
        camera_frame.grid(row=0, column=0, columnspan=2, rowspan=3,sticky="nsew", padx=5, pady=5)

        # Camera widget title bar.
        camera_title_bar = tk.Frame(camera_frame, height=30)
        camera_title_bar.pack(fill="x")

        # Configure title bar
        camera_title_bar.columnconfigure(0, weight=1)
        camera_title_bar.columnconfigure(1, weight=1)
        camera_title_bar.columnconfigure(2, weight=1)

        # Camera widget title label
        camera_widget_label = tk.Label(camera_title_bar,
                                       text="Camera view")
        camera_widget_label.grid(column=0, row=0, sticky="w")

        # Connection status label
        connection_label = tk.Label(camera_title_bar,
                                    text="Disconnected")
        connection_label.grid(column=1, row=0, sticky="nsew")
        self.connection_label = connection_label

        # Camera widget expend btn.
        camera_widget_expend_btn = tk.Button(camera_title_bar,
                                             text=self.square_character,
                                             fg="black",
                                             command= lambda idx=System_id.Camera_id.value: self.open_info_window(idx))
        camera_widget_expend_btn.grid(column=2, row=0, sticky="e")

        #  Camera content frame
        camera_content_frame = tk.Frame(camera_frame, bg="white")
        camera_content_frame.pack(expand=True, fill="both", pady=2)

        #  Configure degrader content grid
        camera_content_frame.rowconfigure(0, weight=1)
        camera_content_frame.rowconfigure(1, weight=200)

        camera_content_frame.columnconfigure(0, weight=1)

        # Degrader ip address frame
        camera_ip_addr_frame = tk.Frame(camera_content_frame, height=30)
        camera_ip_addr_frame.grid(row=0, column=0, padx=1, pady=1, sticky="nsew")
        camera_ip_addr_frame.grid_propagate(False)

        # Degrader ip address frame input
        camera_ip_addr_frame_input = tk.Entry(camera_ip_addr_frame)
        camera_ip_addr_frame_input.pack(side="left", fill="both", expand=True)
        self.camera_ip_addr_frame_input = camera_ip_addr_frame_input

        # Degrader ip address frame submit btn.
        camera_ip_addr_frame_btn = tk.Button(camera_ip_addr_frame,
                                      text="Connect",
                                      command= lambda sys_id=System_id.Camera_id.value, conn_item=connection_label: self.call_start_system(sys_id, connection_label))
        camera_ip_addr_frame_btn.pack(side="right")

        # Camera view frame
        camera_view_frame = tk.Frame(camera_content_frame, bg="blue")
        camera_view_frame.grid(row=1, column=0, sticky="nsew")
        self.camera_view_frame = camera_view_frame      
        # self.camera_view_frame.configure(text="red")

        self.cam = Test_Camera(camera_view_frame , None) 
        self.cam.connected_widget = connection_label

    def add_degrader_control_widget(self):
        # -------------------------------------------------------------------------------
        # Degrader widget
        # lens state 
        # if True degrader lens is in beam path/ is on. False degrader is not in beam path/ is off.
        is_degrader_lens = {
            "2mm": tk.BooleanVar(),
            "3mm": tk.BooleanVar(),
            "6mm": tk.BooleanVar(),
            "8mm": tk.BooleanVar(),
            "10mm": tk.BooleanVar(),
            "12mm": tk.BooleanVar(),
            "30mm": tk.BooleanVar()
        }

        self.is_degrader_lens = is_degrader_lens

        degrader_frame = tk.Frame(self.main_frame, bd=2, relief="ridge")
        degrader_frame.grid(row=0, column=2, padx=10, pady=10,sticky="nsew")
        self.degrader_frame = degrader_frame

        # Degrader widget title bar
        degrader_title_bar = tk.Frame(degrader_frame, height=30)
        degrader_title_bar.pack(fill="x")

        # Configure title bar
        degrader_title_bar.columnconfigure(0, weight=1)
        degrader_title_bar.columnconfigure(1, weight=1)
        degrader_title_bar.columnconfigure(2, weight=1)

        # Degrader expand button
        degrader_expend_button = tk.Button(degrader_title_bar,
                                           text=self.square_character,
                                           fg="black",
                                           width=2,
                                           command=lambda idx=System_id.Degrader_id.value: self.open_info_window(idx))
        degrader_expend_button.grid(row=0, column=2, sticky="e")

        # Degrader title bar text
        degrader_extend_label = tk.Label(degrader_title_bar,
                                         text="Degrader")
        degrader_extend_label.grid(row=0, column=0, sticky="w")

        # Connection status label
        connection_label = tk.Label(degrader_title_bar,
                                    text="Disconnected")
        connection_label.grid(column=1, row=0, sticky="nsew")

        self.sub_system_degrader.degrader_conn_status_frame = connection_label

        # Degrader content frame
        degrader_content_frame = tk.Frame(degrader_frame, bg="white")
        degrader_content_frame.pack(expand=True, fill="both")

        # Configure degrader content grid
        # Grid is 5x6 (rows x cols)
        #Configure the rows 
        # Configure the rows
        # Configure the rows
        for row in range(5):
            degrader_content_frame.grid_rowconfigure(row, weight=1, minsize=35)

        # Configure the columns
        for col in range(6):
            degrader_content_frame.grid_columnconfigure(col, weight=1, minsize=60)

        # Degrader ip address frame
        ip_addr_frame = tk.Frame(degrader_content_frame, height=28)
        ip_addr_frame.grid(row=0, column=0, columnspan=6, padx=5, pady=5, sticky="nsew")

        # Degrader ip address frame input
        degrader_ip_addr_frame_input = tk.Entry(ip_addr_frame)
        degrader_ip_addr_frame_input.pack(side="left", fill="x", expand=True, padx=5)
        self.degrader_ip_addr_frame_input = degrader_ip_addr_frame_input
        # Degrader ip address frame submit btn
        ip_addr_frame_btn = tk.Button(ip_addr_frame,
                                    text="Connect",
                                    command=lambda sys_id=System_id.Degrader_id.value, conn_label=connection_label: self.call_start_system(sys_id, conn_label))
        ip_addr_frame_btn.pack(side="right", padx=5)

        # Degrader lenses
        # 2mm lens
        lens_2mm_btn = tk.Button(degrader_content_frame,
                                text=f'2mm: {button_state[self.lens_btn_state["2mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["2mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("2mm", lens_2mm_btn))
        lens_2mm_btn.grid(row=1, column=0, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 3mm lens
        lens_3mm_btn = tk.Button(degrader_content_frame,
                                text=f'3mm: {button_state[self.lens_btn_state["3mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["3mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("3mm", lens_3mm_btn))
        lens_3mm_btn.grid(row=1, column=2, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 6mm lens
        lens_6mm_btn = tk.Button(degrader_content_frame,
                                text=f'6mm: {button_state[self.lens_btn_state["6mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["6mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("6mm", lens_6mm_btn))
        lens_6mm_btn.grid(row=1, column=4, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 8mm lens
        lens_8mm_btn = tk.Button(degrader_content_frame,
                                text=f'8mm: {button_state[self.lens_btn_state["8mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["8mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("8mm", lens_8mm_btn))
        lens_8mm_btn.grid(row=2, column=0, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 10mm lens
        lens_10mm_btn = tk.Button(degrader_content_frame,
                                text=f'10mm: {button_state[self.lens_btn_state["10mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["10mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("10mm", lens_10mm_btn))
        lens_10mm_btn.grid(row=2, column=2, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 12mm lens
        lens_12mm_btn = tk.Button(degrader_content_frame,
                                text=f'12mm: {button_state[self.lens_btn_state["12mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["12mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("12mm", lens_12mm_btn))
        lens_12mm_btn.grid(row=2, column=4, columnspan=2, padx=5, pady=5, sticky="nsew")

        # 30mm lens
        lens_30mm_btn = tk.Button(degrader_content_frame,
                                text=f'30mm: {button_state[self.lens_btn_state["30mm"]["current"]]["text"]}',
                                bg=button_state[self.lens_btn_state["30mm"]["current"]]["color"],
                                fg="white",
                                activebackground="gray",
                                font=("Arial", 10, "bold"),
                                command=lambda: self.toggle_state("30mm", lens_30mm_btn))
        lens_30mm_btn.grid(row=3, column=0, columnspan=2, padx=5, pady=5, sticky="nsew")

        self.degrader_buttons = {
            "2mm": lens_2mm_btn,
            "3mm": lens_3mm_btn,
            "6mm": lens_6mm_btn,
            "8mm": lens_8mm_btn,
            "10mm": lens_10mm_btn,
            "12mm": lens_12mm_btn,
            "30mm": lens_30mm_btn
        }

        # update button 
        update_lens_btn = tk.Button(degrader_content_frame,
                                text="Update",
                                bg="lightgrey",
                                activebackground="gray",
                                command=self.degrader_update)
        update_lens_btn.grid(row=3, column=4, columnspan=2, padx=5, pady=5, sticky="nsew")

        # Update degrader buttons as commands are being process
        self.root.after(2000, self.updated_degrader_button_states)

    def add_xy_platform_control_widget(self):

        # -------------------------------------------------------------------------------
        # xy_platform widget
        xy_platform_frame = tk.Frame(self.main_frame, bd=2, relief="ridge")
        xy_platform_frame.grid(row=1, column=2, padx=10, pady=10,sticky="nsew")

        # xy_platform widget title bar
        xy_platform_title_bar = tk.Frame(xy_platform_frame, height=30)
        xy_platform_title_bar.pack(fill="x")

        # Configure title bar
        xy_platform_title_bar.columnconfigure(0, weight=1)
        xy_platform_title_bar.columnconfigure(1, weight=1)
        xy_platform_title_bar.columnconfigure(2, weight=1)

        # xy_platform expand button
        xy_platform_expend_button = tk.Button(xy_platform_title_bar,
                                              text=self.square_character,
                                              fg="black",
                                              width=2,
                                              command=lambda idx=System_id.XY_Platform_id.value: self.open_info_window(idx))
        xy_platform_expend_button.grid(row=0, column=2, sticky="e")

        # xy_platform title bar text
        xy_platform_extend_label = tk.Label(xy_platform_title_bar,
                                         text="XY platform")
        xy_platform_extend_label.grid(row=0, column=0, sticky="w")

        # Connection status label
        connection_label = tk.Label(xy_platform_title_bar,
                                    text="Disconnected")
        connection_label.grid(column=1, row=0, sticky="nsew")

        # xy_platform content frame
        xy_platform_content_frame = tk.Frame(xy_platform_frame, bg="white")
        xy_platform_content_frame.pack(expand=True, fill="both")

        # Configure xy_platform content grid
        # Grid is 4x6 (rows x cols)
         # Configure the rows 
        for row in range(5):
             xy_platform_content_frame.grid_rowconfigure(row, weight=1, minsize=29)

        # Configure the column
        for col in range(6):
             xy_platform_content_frame.grid_columnconfigure(col, weight=1, minsize=29)


        # xy_platform ip address frame
        xy_platform_ip_addr_frame = tk.Frame( xy_platform_content_frame, height=28)
        xy_platform_ip_addr_frame.grid(row=0, column=0, columnspan=6, padx=5, pady=5, sticky="nsew")
        self.xy_platform_ip_addr_frame = xy_platform_ip_addr_frame
        #xy_platform ip address frame input
        xy_platform_ip_addr_frame_input = tk.Entry(xy_platform_ip_addr_frame)
        xy_platform_ip_addr_frame_input.pack(side="left", fill="x", expand=True)
        self.xy_platform_ip_addr_frame_input = xy_platform_ip_addr_frame_input

        #xy_platform ip address frame submit btn.
        xy_platform_ip_frame_btn = tk.Button(xy_platform_ip_addr_frame,
                                      text="Connect",
                                      command= lambda sys_id=System_id.XY_Platform_id.value: self.call_start_system(sys_id))
        xy_platform_ip_frame_btn.pack(side="right")

        # position control
        xy_platform_user_input_frame = tk.Frame(xy_platform_content_frame, height=28, bg="white")
        xy_platform_user_input_frame.grid(row=1, rowspan=2,column=0, columnspan=6, padx=5, pady=5, sticky="nsew")

        # Grid config
        xy_platform_user_input_frame.grid_columnconfigure(0,weight=1) #Label column
        xy_platform_user_input_frame.grid_columnconfigure(1,weight=4) # Entry column

        xy_platform_user_input_frame.grid_rowconfigure(0,weight=1) # Horizontal
        xy_platform_user_input_frame.grid_rowconfigure(1,weight=1) # Vertical
        
        # Horizontal control
        xy_horizontal_label = tk.Label(xy_platform_user_input_frame, text="Horizontal Position", bg="white", font=("Arial", 10, "bold"), justify="left")
        xy_horizontal_label.grid(row=0, column=0, sticky="nswe")
        xy_horizontal_entry = tk.Entry(xy_platform_user_input_frame, width=10)
        xy_horizontal_entry.grid(row=0, column=1, sticky="nswe")

        # vertical control
        xy_vertical_label = tk.Label(xy_platform_user_input_frame, text="Vertical Position", bg="white", font=("Arial", 10, "bold"), justify="left")
        xy_vertical_label.grid(row=1, column=0, sticky="nswe")
        xy_vertical_entry = tk.Entry(xy_platform_user_input_frame, width=10)
        xy_vertical_entry.grid(row=1, column=1, sticky="nswe")

        self.xy_vertical_entry = xy_vertical_entry
        self.xy_horizontal_entry = xy_horizontal_entry

        # Feedback values
        feedback_frame = tk.Frame(xy_platform_content_frame)
        feedback_frame.grid(row=3, rowspan=1,column=0, columnspan=6, padx=5, pady=5, sticky="nsew")

        feedback_frame.grid_columnconfigure(0, weight=1)
        feedback_frame.grid_columnconfigure(2, weight=1)
        feedback_frame.grid_columnconfigure(3, weight=1)
        feedback_frame.grid_columnconfigure(4, weight=1)

        horizontal_feedback_label = tk.Label(feedback_frame, text="Horizontal position(mm)", font=("Arial", 10, "bold"))
        horizontal_feedback_label.grid(row=0, column=0, sticky="nsew")
        horizontal_feedback_value = tk.Label(feedback_frame, text="12543", font=("Arial", 10, "bold"))
        horizontal_feedback_value.grid(row=0, column=1, sticky="nsew")

        vertical_feedback_label = tk.Label(feedback_frame, text="Vertical position(mm)", font=("Arial", 10, "bold"))
        vertical_feedback_label.grid(row=0, column=2, sticky="nsew")
        vertical_feedback_value = tk.Label(feedback_frame, text="12543", font=("Arial", 10, "bold"))
        vertical_feedback_value.grid(row=0, column=3, sticky="nsew")

        update_btn = tk.Button(xy_platform_content_frame, text="Update", command = lambda h=xy_horizontal_entry, v=xy_vertical_entry: self.update_xy_platform(h,v))
        update_btn.grid(row=4, column=4, columnspan=3, sticky="nsew", pady=2)

    def add_dlm_control_widget(self):
        # -------------------------------------------------------------------------------
        # blm widget
        blm_frame = tk.Frame(self.main_frame, bd=2, relief="ridge")
        blm_frame.grid(row=2, column=2, padx=10, pady=10,sticky="nsew")

        # blm widget title bar
        blm_title_bar = tk.Frame(blm_frame, height=30)
        blm_title_bar.pack(fill="x")

        # Configure title bar
        blm_title_bar.columnconfigure(0, weight=1)
        blm_title_bar.columnconfigure(1, weight=1)
        blm_title_bar.columnconfigure(2, weight=1)

        # blm expand button
        blm_expend_button = tk.Button(blm_title_bar,
                                     text=self.square_character,
                                     fg="black",
                                     width=2,
                                     command=lambda idx=System_id.BLM_id.value: self.open_info_window(idx))
        blm_expend_button.grid(row=0, column=3, sticky="e")

        # blm title bar text
        blm_extend_label = tk.Label(blm_title_bar,
                                    text="BLM Sensors")
        blm_extend_label.grid(row=0, column=0, sticky="w")

        # Connection status label
        connection_label = tk.Label(blm_title_bar,
                                    text="Disconnected")
        connection_label.grid(column=1, row=0, sticky="nsew")

        # blm content frame
        blm_content_frame = tk.Frame(blm_frame, bg="white")
        blm_content_frame.pack(expand=True, fill="both")
        self.blm_content_frame = blm_content_frame

        # Configure blm content grid
        # Grid is 5x6 (rows x cols)
         # Configure the rows 
        for row in range(4):
            blm_content_frame.grid_rowconfigure(row, weight=1, minsize=29)

        # Configure the column
        for col in range(2):
            blm_content_frame.grid_columnconfigure(col, weight=1, minsize=29)


        # blm ip address frame
        blm_ip_addr_frame = tk.Frame(blm_content_frame, height=28)
        blm_ip_addr_frame.grid(row=0, column=0, columnspan=6, padx=5, pady=5, sticky="nsew")

        #blm ip address frame input
        blm_ip_addr_frame_input = tk.Entry(blm_ip_addr_frame)
        blm_ip_addr_frame_input.pack(side="left", fill="x", expand=True)

        # make ip field available to object
        self.blm_ip_addr_frame_input = blm_ip_addr_frame_input

        #blm ip address frame submit btn.
        blm_ip_frame_btn = tk.Button(blm_ip_addr_frame,
                                      text="Connect",
                                      command= lambda sys_id=System_id.BLM_id.value: self.call_start_system(sys_id, connection_label))
        blm_ip_frame_btn.pack(side="right")

        # Create channel frames and labels in 3x2 grid
        for i in range(6):
            row = (i // 2) + 1  # Rows 1, 2, 3 for channels
            col = i % 2        # Columns 0, 1
            # Channel frame
            channel_frame = tk.Frame(blm_content_frame, bg="white")
            channel_frame.grid(row=row, column=col, padx=5, pady=5, sticky="nsew")
            channel_frame.grid_columnconfigure(0, weight=1)
            channel_frame.grid_columnconfigure(1, weight=1)
            
            # Channel name label (clickable for editing)
            channel_label = tk.Label(channel_frame, text=self.channel_names[i], bg="white", font=("Arial", 10, "bold"))
            channel_label.grid(row=0, column=0, padx=5, pady=2, sticky="w")
            channel_label.bind("<Button-1>", lambda event, idx=i, lbl=channel_label, r=row, c=col, f=channel_frame: self.edit_channel_name(idx, lbl, r, c, f))
            
            # Channel value label
            value_label = tk.Label(channel_frame, text=str(self.channel_values[i]), bg="white", font=("Arial", 10))
            value_label.grid(row=0, column=1, padx=5, pady=2, sticky="e")


        # Update rate container
        # Last row frame
        update_container = tk.Frame(blm_content_frame)
        update_container.grid(row=4, column=0,columnspan=2,sticky="nsew")

        # Ensure update_container resizes with its parent
        update_container.grid_rowconfigure(0, weight=1) 

        # Configure grid columns for 40%/60% split
        update_container.grid_columnconfigure(0, weight=20)  # 20% for log button
        update_container.grid_columnconfigure(1, weight=30)  # 30% for capture_label
        update_container.grid_columnconfigure(2, weight=30)  # 30% for capture_value_label
        update_container.grid_columnconfigure(3, weight=20)  # 20% for update button.
        self.update_container = update_container

        # Log button
        log_btn = tk.Button(update_container, text="Log", bg="#D1D5DB", fg="black")
        log_btn.grid(row=0, column=0, columnspan=1, padx=5, pady=5, sticky="nsew")

        # Static label for Capture Period
        capture_label = tk.Label(update_container, text="Capture Period (ms)", bg="white", font=("Arial", 10), justify="left")
        capture_label.grid(row=0, column=1, sticky="nsew")

        # Editable capture period value
        capture_value = tk.StringVar(value="1000")  # Initial value
        self.capture_value = capture_value
        capture_value_label = tk.Label(update_container, textvariable=capture_value, bg="white", font=("Arial", 10), border=2, borderwidth=3)
        # Bind click event to edit capture value
        capture_value_label.bind("<Button-1>", lambda event, lbl=capture_value_label: self.update_capture_value(lbl))
        capture_value_label.grid(row=0, column=2, sticky="nsew")

        # Update blm state
        blm_update = tk.Button(update_container, text="Update", bg="#D1D5DB", fg="black", command=self.change_blm_period)
        blm_update.grid(row=0, column=3, padx=5, pady=5, sticky="nsew")              

        self.blm_value_labels = []      # will hold the 6 value labels
        self.blm_connection_label = connection_label   # status label in title bar

        # Create channel frames and labels in 3x2 grid
        for i in range(6):
            row = (i // 2) + 1
            col = i % 2
            channel_frame = tk.Frame(blm_content_frame, bg="white")
            channel_frame.grid(row=row, column=col, padx=5, pady=5, sticky="nsew")
            channel_frame.grid_columnconfigure(0, weight=1)
            channel_frame.grid_columnconfigure(1, weight=1)

            # Channel name label (clickable)
            channel_label = tk.Label(channel_frame,
                                     text=self.channel_names[i],
                                     bg="white",
                                     font=("Arial", 10, "bold"))
            channel_label.grid(row=0, column=0, padx=5, pady=2, sticky="w")
            channel_label.bind("<Button-1>",
                               lambda e, idx=i, lbl=channel_label, r=row, c=col, f=channel_frame:
                               self.edit_channel_name(idx, lbl, r, c, f))

            # Channel value label (will be refreshed live)
            value_label = tk.Label(channel_frame,
                                   text="0",
                                   bg="white",
                                   font=("Arial", 10))
            value_label.grid(row=0, column=1, padx=5, pady=2, sticky="e")
            self.blm_value_labels.append(value_label)

        self.logging_active = False
        self.log_file_handle = None
        self.log_filename = None

        log_btn = tk.Button(update_container,
                            text="Log",
                            bg="#D1D5DB",
                            fg="black",
                            command=self.toggle_blm_logging)
        log_btn.grid(row=0, column=0, columnspan=1, padx=5, pady=5, sticky="nsew")
        self.log_btn = log_btn  # keep reference

    def start_new_log_file(self):
        """Create a new CSV log file with timestamped name and custom channel headings."""
        now = datetime.now()
        filename = now.strftime("%Y_%m_%d_%H_%M_%S.csv")
        self.log_filename = filename

        # Open file
        self.log_file_handle = open(filename, 'w', newline='')

        # --- Build header using current channel names ---
        with self.channel_names_lock:
            header_names = self.channel_names  # e.g. ["PMT-1", "PMT-2", ...]

        header = "timestamp," + ",".join(header_names) + "\n"
        self.log_file_handle.write(header)
        self.log_file_handle.flush()

        print(f"[BLM] Logging started → {filename} (channels: {', '.join(header_names)})")
        return filename

    def stop_logging(self):
        """Close current log file and reset state."""
        if self.log_file_handle:
            self.log_file_handle.close()
            self.log_file_handle = None
        self.logging_active = False
        print(f"[BLM] Logging stopped.")

    def toggle_blm_logging(self):
        """Toggle logging on/off and update UI."""
        if not self.sub_system_blm.running:
            print("[BLM] Cannot log: server not running.")
            return

        if self.logging_active:
            # --- Turn OFF ---
            self.stop_logging()
            self.log_btn.config(text="Log", bg="#D1D5DB", fg="black")
        else:
            # --- Turn ON ---
            filename = self.start_new_log_file()
            self.logging_active = True
            self.log_btn.config(text="Log Off", bg="red", fg="white")

            # Pass the file handle to Dosimeter
            self.sub_system_blm.set_log_file(self.log_file_handle, filename)

    def update_channel_name(self, index, entry, label):
        """Update channel name when Enter is pressed."""
        new_name = entry.get()
        if new_name.strip():  # Ensure non-empty name
            self.channel_names[index] = new_name
        label.config(text=self.channel_names[index])
        entry.destroy()  # Remove entry widget after update

    def edit_channel_name(self, index, label, row, col, frame):
        """Replace label with entry widget for editing."""
        entry = tk.Entry(frame, width=10)
        entry.insert(0, self.channel_names[index])
        entry.grid(row=0, column=0, padx=5, pady=2, sticky="w")
        entry.focus()
        entry.bind("<Return>", lambda event: self.update_channel_name(index, entry, label))

    def update_capture_value(self, label):
        parent = label.master
        x, y, w, h = label.winfo_x(), label.winfo_y(), label.winfo_width(), label.winfo_height()

        entry = tk.Entry(parent, width=10)
        entry.place(x=x, y=y, width=w, height=h)
        entry.insert(0, self.capture_value.get())
        entry.select_range(0, tk.END)
        entry.focus()

        def finish():
            val = entry.get().strip()
            if val.isdigit():
                self.capture_value.set(val)
            entry.destroy()
            self.blm_entry = None

        entry.bind("<Return>", lambda e: finish())
        entry.bind("<FocusOut>", lambda e: finish())
        entry.bind("<Escape>", lambda e: entry.destroy())

        self.blm_entry = entry

    def update_value(self):
        """Update capture value when Enter is pressed."""
        new_value = self.blm_entry.get()
        if new_value.strip() and new_value.isdigit():  # Ensure non-empty and numeric
            self.capture_value.set(new_value)
        self.blm_entry.destroy()  # Remove entry widget after update
        self.blm_entry = None

    def open_info_window(self, index):
        """Open a full-featured pop-up window for the selected subsystem."""
        system_id = System_id(index)

        # Create toplevel window
        win = tk.Toplevel(self.root)
        win.title(f"{system_id.name.replace('_', ' ')} - Detail View")
        win.geometry("560x440")
        win.configure(bg="#f0f0f0")

        # ------------------------------------------------------------------
        # CAMERA - USING YOUR Test_Camera CLASS
        # ------------------------------------------------------------------
        if system_id == System_id.Camera_id:
            frame = tk.Frame(win, bg="white")
            frame.pack(fill="both", expand=True, padx=10, pady=10)

            # === IP + Connect ===
            ip_frame = tk.Frame(frame)
            ip_frame.pack(fill="x", pady=5)

            entry = tk.Entry(ip_frame, width=30)
            entry.insert(0, self.camera_ip_addr_frame_input.get())
            entry.pack(side="left", fill="x", expand=True, padx=(0, 5))

            status_label = tk.Label(ip_frame, text="Disconnected", fg="red")
            status_label.pack(side="right", padx=5)

            connect_btn = tk.Button(ip_frame, text="Connect")
            connect_btn.pack(side="right", padx=5)

            # === Camera View ===
            view_frame = tk.Frame(frame, bg="black")
            view_frame.pack(fill="both", expand=True, pady=5)

            self.cam.parent = view_frame            

            # === Connect function ===
            def connect_camera():
                ip = entry.get().strip()
                
                # Update main UI
                self.camera_ip_addr_frame_input.delete(0, tk.END)
                self.camera_ip_addr_frame_input.insert(0, ip)

                # Update camera
                self.cam.jpeg_url = f'http://{ip}/snap.jpeg'

                # Start new capture
                self.cam.capture()

            connect_btn.config(command=connect_camera)

            # Auto-connect if IP is already set
            if entry.get().strip():
                connect_btn.invoke()

                
        # ------------------------------------------------------------------
        # DEGRADER
        # ------------------------------------------------------------------
        elif system_id == System_id.Degrader_id:
            frame = self._create_degrader_popup(win)
            frame.pack(fill="both", expand=True, padx=10, pady=10)

        # ------------------------------------------------------------------
        # XY PLATFORM
        # ------------------------------------------------------------------
        elif system_id == System_id.XY_Platform_id:
            frame = self._create_xy_platform_popup(win)
            frame.pack(fill="both", expand=True, padx=10, pady=10)

        # ------------------------------------------------------------------
        # BLM
        # ------------------------------------------------------------------
        elif system_id == System_id.BLM_id:
            frame = self._create_blm_popup(win)
            frame.pack(fill="both", expand=True, padx=10, pady=10)

        # Close button
        tk.Button(win, text="Close", command=win.destroy).pack(pady=5)

    def process_lens(self):
        self.sub_system_degrader.send_command()

    def close_widget(self, idx):
        print(f'Close widget {idx}')

    def minimise_widget(self, idx):
        print(f'Minimise widget {idx}')

    def call_start_system(self, sys_id, connection_label):
        if(sys_id == System_id.BLM_id.value):
            blm_ip_addr = self.blm_ip_addr_frame_input.get()
            print(f'Connecting to:{sys_id} with ip address:{blm_ip_addr}')
            blm_ip_addr = self.blm_ip_addr_frame_input.get().strip()
            if not blm_ip_addr:
                blm_ip_addr = "0.0.0.0"   # fallback

            # Store the IP for later commands
            self.sub_system_blm.dosimeter_ip = blm_ip_addr

            # Start the UDP listener (non-blocking background thread)
            self.sub_system_blm.start_server()

            # Update UI
            if connection_label:
                connection_label.config(text="Connected", fg="green")
            self.blm_connection_label = connection_label

            # Kick-off live UI refresh
            self.root.after(100, self._refresh_blm_values)

        elif(sys_id == System_id.Degrader_id.value):
            degrader_ip = self.degrader_ip_addr_frame_input.get()
            self.start_system(sys_id, degrader_ip, connection_label)

        elif(sys_id == System_id.XY_Platform_id.value):
            xy_platform_ip_addr = self.xy_platform_ip_addr_frame_input.get()
            self.start_system(sys_id, xy_platform_ip_addr)

        elif(sys_id == System_id.Camera_id.value):
            
            camera_ip_addr = self.camera_ip_addr_frame_input.get()
            self.cam.camera_ip = camera_ip_addr
            self.cam.rtsp_url = f'http://{camera_ip_addr}/snap.jpeg'
            self.cam.capture()

                
        
    def start_system(self, sys_id, ip_address, connection_label):

        if(sys_id == System_id.BLM_id.value):
            print(f'Connecting to: system{sys_id} with ip address:{ip_address}')            
            pass

        elif(sys_id == System_id.Degrader_id.value):
            
            print(f'Connecting to:{sys_id} with ip address:{ip_address}')
            self.sub_system_degrader.start_degrader(None, ip_address)
            self.updated_degrader_button_states()
            pass

        elif(sys_id == System_id.XY_Platform_id.value):
            print(f'Connecting to:{sys_id} with ip address:{ip_address}')
            pass

        elif(sys_id == System_id.Camera_id.value):
            print(f'Connecting to:{sys_id} with ip address:{ip_address}')
            pass
    
    def update_xy_platform(self, horizontal_entry, vertical_entry):
        horizontal_pos = horizontal_entry.get()
        vertical_pos = vertical_entry.get()

        self.sub_system_xy_platform.send_new_pos(horizontal_pos, vertical_pos)

    def change_blm_period(self):
        if self.blm_entry is not None:
            self.update_value()
        try:
            ms = int(self.capture_value.get())
        except ValueError:
            return
        s = max(ms // 1000, 1)
        self.sub_system_blm.blm_send_new_period(s)
        print(f"[UI] BLM period → {s}s ({ms}ms)")

    def toggle_state(self, btn_name, btn_widget: tk.Widget):
        
        if(self.lens_btn_state[btn_name]["current"] == "off"):
            btn_widget.configure(text=f'{btn_name}: {button_state["to_on"]["text"]}')
            btn_widget.configure(bg=button_state["to_on"]["color"])
            btn_widget.configure(fg="white")
            self.lens_btn_state[btn_name]["previous"] = self.lens_btn_state[btn_name]["current"]
            self.lens_btn_state[btn_name]["current"] = "to_on"
            self.lens_btn_state[btn_name]["desired_state"] = 1 

        elif(self.lens_btn_state[btn_name]["current"] == "on"):
            btn_widget.configure(text=f'{btn_name}: {button_state["to_off"]["text"]}')
            btn_widget.configure(bg=button_state["to_off"]["color"])
            btn_widget.configure(fg="white")
            self.lens_btn_state[btn_name]["previous"] = self.lens_btn_state[btn_name]["current"]
            self.lens_btn_state[btn_name]["current"] = "to_off"
            self.lens_btn_state[btn_name]["desired_state"] = 0

        elif(self.lens_btn_state[btn_name]["current"] == "to_on" or self.lens_btn_state[btn_name]["current"] == "to_off"):
            btn_widget.configure(text=f'{btn_name}: {button_state[self.lens_btn_state[btn_name]["previous"]]["text"]}')
            btn_widget.configure(bg=button_state[self.lens_btn_state[btn_name]["previous"]]["color"])
            btn_widget.configure(fg="white")
            self.lens_btn_state[btn_name]["current"] = self.lens_btn_state[btn_name]["previous"]
            self.lens_btn_state[btn_name]["previous"] = self.lens_btn_state[btn_name]["current"]
            self.lens_btn_state[btn_name]["desired_state"] = int(not self.lens_btn_state[btn_name]["desired_state"])
   
    def updated_degrader_button_states(self):
        try:
            self.root.after(50, self.updated_degrader_button_states)
            with self.sub_system_degrader.response_lock:
                    current_lens_state :Response = self.sub_system_degrader.resp
            if(current_lens_state.process_status == 1 or current_lens_state.process_status == 3):
                self.updating_degrader = False
            

            if(self.updating_degrader):                
                
                self.lens_btn_state["2mm"]["current"] = lens_state[current_lens_state.lens_status_2mm]
                self.lens_btn_state["3mm"]["current"] = lens_state[current_lens_state.lens_status_3mm]
                self.lens_btn_state["6mm"]["current"] = lens_state[current_lens_state.lens_status_6mm]
                self.lens_btn_state["8mm"]["current"] = lens_state[current_lens_state.lens_status_8mm]
                self.lens_btn_state["10mm"]["current"] = lens_state[current_lens_state.lens_status_10mm]
                self.lens_btn_state["12mm"]["current"] = lens_state[current_lens_state.lens_status_12mm]
                self.lens_btn_state["30mm"]["current"] = lens_state[current_lens_state.lens_status_30mm]

                for button in self.degrader_buttons:
                    currentText = button_state[self.lens_btn_state[button]["current"]]["text"] 
                    bg_color = button_state[self.lens_btn_state[button]["current"]]["color"]
                    self.degrader_buttons[button].configure(text=f'{currentText}')
                    self.degrader_buttons[button].configure(bg=bg_color)
                    self.degrader_buttons[button].configure(fg="white")

            

        except Exception as e:
            print("Failed to update buttons", ":", e)
            
    def degrader_update(self):
        self.sub_system_degrader.set_command(self.lens_btn_state)
        self.sub_system_degrader.send_command()
        self.updating_degrader = True

    def _refresh_blm_values(self):
        """Called every ~100 ms – updates the 6 value labels."""
        if not hasattr(self.sub_system_blm, 'latest_counts'):
            # Dosimeter has not received anything yet
            self.root.after(100, self._refresh_blm_values)
            return

        # Grab a copy under the lock (the server thread writes to it)
        with self.sub_system_blm.counts_lock:
            counts = self.sub_system_blm.latest_counts[:]   # shallow copy

        for i, val in enumerate(counts):
            if i < len(self.blm_value_labels):
                self.blm_value_labels[i].config(text=str(val))

        # Keep scheduling
        self.root.after(100, self._refresh_blm_values)

    def _create_degrader_popup(self, parent):
        frame = tk.Frame(parent, bg="white", relief="ridge", bd=2)
        lens_state_copy = {k: v.copy() for k, v in self.lens_btn_state.items()}
        buttons = {}

        # Title
        tk.Label(frame, text="Degrader Control", font=("Arial", 12, "bold"), bg="white").pack(pady=5)

        # Grid: 4 rows × 6 cols
        content = tk.Frame(frame, bg="white")
        content.pack(expand=True, fill="both")
        for r in range(5):
            content.grid_rowconfigure(r, weight=1)
        for c in range(6):
            content.grid_columnconfigure(c, weight=1)

        # IP + Connect
        ip_frame = tk.Frame(content)
        ip_frame.grid(row=0, column=0, columnspan=6, sticky="ew", pady=2)
        entry = tk.Entry(ip_frame)
        entry.insert(0, self.degrader_ip_addr_frame_input.get())
        entry.pack(side="left", fill="x", expand=True, padx=2)
        tk.Button(ip_frame, text="Connect",
                command=lambda: self._connect_degrader_popup(entry)).pack(side="right", padx=2)

        # Lens buttons
        lenses = ["2mm", "3mm", "6mm", "8mm", "10mm", "12mm", "30mm"]
        positions = [(1,0,2), (1,2,2), (1,4,2), (2,0,2), (2,2,2), (2,4,2), (3,0,2)]
        for lens, (r, c, cs) in zip(lenses, positions):
            btn = tk.Button(content,
                            text=f"{lens}: Off beam",
                            bg="green", fg="white",
                            command=lambda l=lens: self._toggle_lens_popup(l, buttons, lens_state_copy))
            btn.grid(row=r, column=c, columnspan=cs, sticky="nsew", padx=2, pady=2)
            buttons[lens] = btn

        # Update button
        tk.Button(content, text="Update", bg="lightgrey",
                command=lambda: self._update_degrader_popup(lens_state_copy)).grid(
            row=3, column=4, columnspan=2, sticky="nsew", padx=2, pady=2)

        # Sync initial state
        self._sync_degrader_buttons(buttons, lens_state_copy)
        self.root.after(100, lambda: self._poll_degrader_state(buttons, lens_state_copy))

        return frame

    def _connect_degrader_popup(self, entry):
        ip = entry.get().strip()
        self.degrader_ip_addr_frame_input.delete(0, tk.END)
        self.degrader_ip_addr_frame_input.insert(0, ip)
        self.start_system(System_id.Degrader_id.value, ip, None)

    def _toggle_lens_popup(self, lens, buttons, state_dict):
        current = state_dict[lens]["current"]
        if current == "off":
            state_dict[lens]["current"] = "to_on"
            state_dict[lens]["desired_state"] = 1
        elif current == "on":
            state_dict[lens]["current"] = "to_off"
            state_dict[lens]["desired_state"] = 0
        elif current in ("to_on", "to_off"):
            state_dict[lens]["current"] = state_dict[lens]["previous"]
            state_dict[lens]["desired_state"] = 1 - state_dict[lens]["desired_state"]
        self._update_single_button(buttons[lens], lens, state_dict)

    def _update_degrader_popup(self, state_dict):
        self.lens_btn_state = state_dict
        self.degrader_update()

    def _sync_degrader_buttons(self, buttons, state_dict):
        for lens, btn in buttons.items():
            self._update_single_button(btn, lens, state_dict)

    def _update_single_button(self, btn, lens, state_dict):
        s = state_dict[lens]
        text = button_state[s["current"]]["text"]
        color = button_state[s["current"]]["color"]
        btn.config(text=f"{lens}: {text}", bg=color, fg="white")

    def _poll_degrader_state(self, buttons, state_dict):
        if not self.root.winfo_exists():
            return
        try:
            with self.sub_system_degrader.response_lock:
                resp = self.sub_system_degrader.resp
            for lens in ["2mm","3mm","6mm","8mm","10mm","12mm","30mm"]:
                status = getattr(resp, f"lens_status_{lens}")
                state_dict[lens]["current"] = lens_state[status]
            self._sync_degrader_buttons(buttons, state_dict)
        except:
            pass
        self.root.after(200, lambda: self._poll_degrader_state(buttons, state_dict))

    def _create_xy_platform_popup(self, parent):
        frame = tk.Frame(parent, bg="white", relief="ridge", bd=2)
        tk.Label(frame, text="XY Platform Control", font=("Arial", 12, "bold"), bg="white").pack(pady=5)

        content = tk.Frame(frame, bg="white")
        content.pack(expand=True, fill="both", padx=10, pady=5)
        for r in range(5):
            content.grid_rowconfigure(r, weight=1)
        for c in range(6):
            content.grid_columnconfigure(c, weight=1)

        # IP
        ip_frame = tk.Frame(content)
        ip_frame.grid(row=0, column=0, columnspan=6, sticky="ew", pady=2)
        entry = tk.Entry(ip_frame)
        entry.insert(0, self.xy_platform_ip_addr_frame_input.get())
        entry.pack(side="left", fill="x", expand=True, padx=2)
        tk.Button(ip_frame, text="Connect",
                  command=lambda: self._connect_xy_popup(entry)).pack(side="right", padx=2)

        # Position inputs
        tk.Label(content, text="Horizontal (mm)", bg="white").grid(row=1, column=0, columnspan=3, sticky="w")
        h_entry = tk.Entry(content)
        h_entry.grid(row=1, column=3, columnspan=3, sticky="ew")
        tk.Label(content, text="Vertical (mm)", bg="white").grid(row=2, column=0, columnspan=3, sticky="w")
        v_entry = tk.Entry(content)
        v_entry.grid(row=2, column=3, columnspan=3, sticky="ew")

        # Feedback
        tk.Label(content, text="Current H:", bg="white").grid(row=3, column=0, columnspan=2, sticky="e")
        h_fb = tk.Label(content, text="—", bg="white", relief="sunken")
        h_fb.grid(row=3, column=2, columnspan=2, sticky="ew")
        tk.Label(content, text="Current V:", bg="white").grid(row=3, column=3, columnspan=2, sticky="e")
        v_fb = tk.Label(content, text="—", bg="white", relief="sunken")
        v_fb.grid(row=3, column=5, columnspan=1, sticky="ew")

        # Update
        tk.Button(content, text="Update", command=lambda: self.update_xy_platform(h_entry, v_entry)).grid(
            row=4, column=4, columnspan=2, sticky="nsew", pady=5)

        return frame

    def _connect_xy_popup(self, entry):
        ip = entry.get().strip()
        self.xy_platform_ip_addr_frame_input.delete(0, tk.END)
        self.xy_platform_ip_addr_frame_input.insert(0, ip)
        self.start_system(System_id.XY_Platform_id.value, ip)

    def _create_blm_popup(self, parent):
        frame = tk.Frame(parent, bg="white", relief="ridge", bd=2)
        tk.Label(frame, text="BLM Sensors", font=("Arial", 12, "bold"), bg="white").pack(pady=5)

        content = tk.Frame(frame, bg="white")
        content.pack(expand=True, fill="both", padx=10, pady=5)
        for r in range(5):
            content.grid_rowconfigure(r, weight=1)
        for c in range(2):
            content.grid_columnconfigure(c, weight=1)

        # IP
        ip_frame = tk.Frame(content)
        ip_frame.grid(row=0, column=0, columnspan=2, sticky="ew", pady=2)
        entry = tk.Entry(ip_frame)
        entry.insert(0, self.blm_ip_addr_frame_input.get())
        entry.pack(side="left", fill="x", expand=True, padx=2)
        tk.Button(ip_frame, text="Connect",
                  command=lambda: self._connect_blm_popup(entry)).pack(side="right", padx=2)

        # Channels
        value_labels = []
        for i in range(6):
            row = (i // 2) + 1
            col = i % 2
            ch_frame = tk.Frame(content, bg="white")
            ch_frame.grid(row=row, column=col, sticky="nsew", padx=2, pady=2)

            name = self.channel_names[i]
            name_lbl = tk.Label(ch_frame, text=name, bg="white", font=("Arial", 10, "bold"))
            name_lbl.grid(row=0, column=0, sticky="w")
            name_lbl.bind("<Button-1>", lambda e, idx=i, lbl=name_lbl: self._edit_channel_popup(idx, lbl))

            val_lbl = tk.Label(ch_frame, text="0", bg="white", font=("Arial", 10))
            val_lbl.grid(row=0, column=1, sticky="e")
            value_labels.append(val_lbl)

        # Capture + Log
        ctrl = tk.Frame(content)
        ctrl.grid(row=5, column=0, columnspan=2, sticky="ew", pady=5)
        log_btn = tk.Button(ctrl, text="Log", command=lambda: self.toggle_blm_logging())
        log_btn.pack(side="left", padx=5)
        self.popup_log_btn = log_btn

        tk.Label(ctrl, text="Period (ms)").pack(side="left")
        period_var = tk.StringVar(value="1000")
        period_lbl = tk.Label(ctrl, textvariable=period_var, relief="sunken", width=8)
        period_lbl.pack(side="left", padx=2)
        period_lbl.bind("<Button-1>", lambda e: self._edit_period_popup(period_var, period_lbl))

        tk.Button(ctrl, text="Update", command=lambda: self._apply_blm_period(period_var)).pack(side="right", padx=5)

        # --- NEW: Store after ID and bind destroy ---
        self.blm_popup_after_id = None
        self.blm_popup_value_labels = value_labels
        self.blm_popup_window = parent

        # Bind destroy event to clean up
        parent.protocol("WM_DELETE_WINDOW", self._on_blm_popup_close)

        # Start polling
        self._start_blm_popup_polling()

        return frame
    
    def _start_blm_popup_polling(self):
        """Start safe polling loop."""
        if not hasattr(self, 'blm_popup_window') or not self.blm_popup_window.winfo_exists():
            return
        self._poll_blm_popup_safe()
        self.blm_popup_after_id = self.root.after(100, self._start_blm_popup_polling)

    def _poll_blm_popup_safe(self):
        """Update labels only if they still exist."""
        if not hasattr(self, 'blm_popup_value_labels'):
            return

        # Check if any label is still alive
        labels = [lbl for lbl in self.blm_popup_value_labels if self._widget_exists(lbl)]
        if not labels:
            return  # All destroyed

        if hasattr(self.sub_system_blm, 'latest_counts'):
            with self.sub_system_blm.counts_lock:
                counts = self.sub_system_blm.latest_counts[:6]  # safety

            for lbl, val in zip(labels, counts):
                if self._widget_exists(lbl):
                    try:
                        lbl.config(text=str(val))
                    except tk.TclError:
                        pass  # widget gone mid-update

    def _widget_exists(self, widget):
        """Check if Tk widget still exists."""
        try:
            widget.winfo_exists()
            return True
        except tk.TclError:
            return False

    def _on_blm_popup_close(self):
        """Cancel polling when popup is closed."""
        if hasattr(self, 'blm_popup_after_id') and self.blm_popup_after_id:
            try:
                self.root.after_cancel(self.blm_popup_after_id)
            except:
                pass
            self.blm_popup_after_id = None

        if hasattr(self, 'blm_popup_window'):
            try:
                self.blm_popup_window.destroy()
            except:
                pass
            self.blm_popup_window = None
            self.blm_popup_value_labels = []

    def _connect_blm_popup(self, entry):
        ip = entry.get().strip() 
        self.blm_ip_addr_frame_input.delete(0, tk.END)
        self.blm_ip_addr_frame_input.insert(0, ip)
        self.call_start_system(System_id.BLM_id.value, None)

    def _edit_channel_popup(self, idx, label):
        entry = tk.Entry(label.master)
        entry.insert(0, self.channel_names[idx])
        entry.grid(row=0, column=0, sticky="w")
        entry.focus()
        entry.bind("<Return>", lambda e: self._save_channel_popup(idx, entry, label))

    def _save_channel_popup(self, idx, entry, label):
        name = entry.get().strip() or f"Channel {idx+1}"
        self.channel_names[idx] = name
        label.config(text=name)
        entry.destroy()
        self.sync_channel_names_to_dosimeter()

    def _edit_period_popup(self, var, label):
        """Replace the period label with an Entry for in-place editing."""
        # Get parent and geometry
        parent = label.master
        x, y, w, h = label.winfo_x(), label.winfo_y(), label.winfo_width(), label.winfo_height()

        # Create Entry
        entry = tk.Entry(parent, textvariable=var, width=8, justify='center')
        entry.place(x=x, y=y, width=w, height=h)  # overlay exactly
        entry.select_range(0, tk.END)
        entry.focus()
        entry.icursor(tk.END)

        # Bind events
        entry.bind("<Return>", lambda e: self._finish_period_edit(entry, label))
        entry.bind("<FocusOut>", lambda e: self._finish_period_edit(entry, label))
        entry.bind("<Escape>", lambda e: self._finish_period_edit(entry, label, revert=True))

        # Store for cleanup
        self._current_period_entry = entry

    def _finish_period_edit(self, entry, label, revert=False):
        """Restore label after editing."""
        if revert:
            pass  # var already has old value
        else:
            try:
                val = entry.get().strip()
                if val.isdigit():
                    self.capture_value.set(val)  # sync with main UI
            except:
                pass

        # Destroy entry and restore label
        try:
            entry.destroy()
        except:
            pass
        if hasattr(self, '_current_period_entry'):
            del self._current_period_entry

        # Re-show label
        label.update_idletasks()

    def _apply_blm_period(self, var):
        try:
            ms = int(var.get())
            s = max(ms // 1000, 1)
            self.sub_system_blm.blm_send_new_period(s)
        except:
            pass

    def _poll_blm_popup(self, labels):
        if not self.root.winfo_exists():
            return
        if hasattr(self.sub_system_blm, 'latest_counts'):
            with self.sub_system_blm.counts_lock:
                counts = self.sub_system_blm.latest_counts[:]
            for lbl, val in zip(labels, counts):
                lbl.config(text=str(val))
        self.root.after(100, lambda: self._poll_blm_popup(labels))

    def on_app_close(self):
        # Stop logging first
        if self.logging_active:
            self.stop_logging()
            self.log_btn.config(text="Log", bg="#D1D5DB", fg="black")

        if hasattr(self.sub_system_blm, 'running') and self.sub_system_blm.running:
            self.sub_system_blm.stop_server()

        self.sub_system_degrader.degrader_close()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = MainWidget(root)

    root.mainloop()

    