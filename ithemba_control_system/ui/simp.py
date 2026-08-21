 # Configure xy_platform content grid
        # Grid is 4x7 (rows x cols)
         # Configure the rows 
        for row in range(4):
             xy_platform_content_frame.grid_rowconfigure(row, weight=1, minsize=29)

        # Configure the column
        for col in range(7):
             xy_platform_content_frame.grid_columnconfigure(col, weight=1, minsize=29)


        # xy_platform ip address frame
        xy_platform_ip_addr_frame = tk.Frame( xy_platform_content_frame, height=28)
        xy_platform_ip_addr_frame.grid(row=0, column=0, columnspan=7, padx=5, pady=5, sticky="nsew")

        #xy_platform ip address frame input
        xy_platform_ip_addr_frame_input = tk.Entry(xy_platform_ip_addr_frame)
        xy_platform_ip_addr_frame_input.pack(side="left", fill="x", expand=True)

        #xy_platform ip address frame submit btn.
        xy_platform_ip_frame_btn = tk.Button(xy_platform_ip_addr_frame,
                                      text="Connect",
                                      command= lambda sys_id=System_id.XY_Platform_id.value: call_start_sytem(sys_id))
        xy_platform_ip_frame_btn.pack(side="right")