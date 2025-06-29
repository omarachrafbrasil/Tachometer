#!/usr/bin/env python3
"""
File: TachometerGUI.py
Author: Omar Achraf / omarachraf@gmail.com
Date: 2025-06-28
Version: 1.0
Description: Advanced GUI interface for Industrial Tachometer using Tkinter.
             Provides real-time data visualization, parameter control, and system monitoring.
             Communicates with Arduino via Serial protocol for complete system control.
             
pip install matplotlib             
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, filedialog
import serial
import serial.tools.list_ports
import threading
import time
import queue
import json
from datetime import datetime
from collections import deque

import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg as FigureCanvasTkinter
from matplotlib.figure import Figure
import matplotlib.animation as animation

from BrazilianFormatter import (
    format_rpm_br, 
    format_frequency_br, 
    format_time_br, 
    format_revolutions_br
)

from Log import Log, LogLevel


"""
Class responsible for advanced GUI control of Industrial Tachometer System.
Provides real-time monitoring, parameter adjustment, and data visualization
with thread-safe communication to Arduino via Serial protocol.
"""
class TachometerGUI:
    APP_TITTLE = "Industrial Tachometer - v1.0 - Omar Achraf - omarachraf@gmail.com - UVSBR"

    """
    Initialize GUI application and setup all components.
    """
    def __init__(self):
        header = "TachometerGUI.__init__()"
        
        # Initialize logging system first
        Log.set_level(LogLevel.DEBUG)
        Log.set_log_file("tachometer_gui.log", append=True)
        Log.set_console_output(True)
        
        Log.trace(f"{header}: Starting Tachometer GUI application")
        
        # Serial communication variables
        self.serial_port = None
        self.serial_thread = None
        self.data_queue = queue.Queue()
        self.command_queue = queue.Queue()
        self.is_connected = False
        self.stop_threads = False
        
        # Data storage for plotting
        self.max_data_points = 100
        self.time_data = deque(maxlen=self.max_data_points)
        self.rpm_data = deque(maxlen=self.max_data_points)
        self.filtered_rpm_data = deque(maxlen=self.max_data_points)
        self.frequency_data = deque(maxlen=self.max_data_points)
        
        # Current measurements
        self.current_data = {
            'frequency': 0,
            'rpm': 0,
            'filtered_freq': 0,
            'filtered_rpm': 0,
            'total_revs': 0,
            'raw_pulses': 0,
            'pulse_interval': 0,
            'timestamp': 0
        }
        
        # System configuration
        self.config = {
            'ir_pin': 2,
            'sample_period': 1000,
            'debounce_time': 100,
            'pulses_per_rev': 1,
            'timer_number': 1,
            'filtering_enabled': True,
            'filter_alpha': 800,
            'window_size': 5
        }
        
        # Create main window
        self.create_main_window()
        self.create_widgets()
        self.setup_plotting()
        
        # Start GUI update timer
        self.update_gui_timer()
        
        Log.info(f"{header}: GUI initialization complete")


    """
    Create and configure the main application window.
    """
    def create_main_window(self):
        header = "TachometerGUI.create_main_window()"
        Log.trace(f"{header}: Creating main application window")
        
        self.root = tk.Tk()
        self.root.title(self.APP_TITTLE)
        self.root.geometry("1000x800")
        self.root.minsize(1000, 800)
        
        # Configure style
        self.style = ttk.Style()
        self.style.theme_use('clam')
        
        # Configure colors
        self.colors = {
            'bg_primary': '#2c3e50',
            'bg_secondary': '#34495e',
            'accent': '#3498db',
            'success': '#27ae60',
            'warning': '#f39c12',
            'danger': '#e74c3c',
            'text': '#ecf0f1'
        }
        
        # Set window properties
        self.root.configure(bg=self.colors['bg_primary'])
        
        # Handle window closing
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        Log.debug(f"{header}: Main window configured successfully")


    """
    Create and layout all GUI widgets.
    """
    def create_widgets(self):
        header = "TachometerGUI.create_widgets()"
        Log.trace(f"{header}: Creating GUI widgets")
        
        # Create main notebook for tabs
        self.notebook = ttk.Notebook(self.root)
        self.notebook.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Create tabs
        self.create_monitor_tab()
        self.create_control_tab()
        self.create_config_tab()
        self.create_log_tab()
        
        Log.debug(f"{header}: All GUI widgets created successfully")


    """
    Create real-time monitoring tab with displays and charts.
    """
    def create_monitor_tab(self):
        header = "TachometerGUI.create_monitor_tab()"
        Log.trace(f"{header}: Creating monitoring tab")
        
        # Monitor tab frame
        self.monitor_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.monitor_frame, text="📊 Real-Time Monitor")
        
        # Connection status frame
        status_frame = ttk.LabelFrame(self.monitor_frame, text="Connection Status", padding=10)
        status_frame.grid(row=0, column=0, columnspan=2, sticky="ew", padx=5, pady=5)
        
        # Serial port selection
        ttk.Label(status_frame, text="Serial Port:").grid(row=0, column=0, sticky="w")
        self.port_combo = ttk.Combobox(status_frame, width=15, state="readonly")
        self.port_combo.grid(row=0, column=1, padx=5)
        
        ttk.Button(status_frame, text="Refresh Ports", 
                  command=self.refresh_ports).grid(row=0, column=2, padx=5)
        
        self.connect_btn = ttk.Button(status_frame, text="Connect", 
                                     command=self.toggle_connection)
        self.connect_btn.grid(row=0, column=3, padx=5)
        
        # Connection status indicator
        self.status_label = ttk.Label(status_frame, text="Disconnected")
        self.status_label.grid(row=0, column=4, padx=10)
        
        # Real-time measurements frame
        measurements_frame = ttk.LabelFrame(self.monitor_frame, text="Current Measurements", padding=10)
        measurements_frame.grid(row=1, column=0, sticky="nsew", padx=5, pady=5)
        
        # Create measurement displays
        self.create_measurement_displays(measurements_frame)
        
        # Chart frame
        chart_frame = ttk.LabelFrame(self.monitor_frame, text="Real-Time Charts", padding=5)
        chart_frame.grid(row=1, column=1, sticky="nsew", padx=5, pady=5)
        self.chart_frame = chart_frame
        
        # Configure grid weights
        self.monitor_frame.columnconfigure(0, weight=1)
        self.monitor_frame.columnconfigure(1, weight=2)
        self.monitor_frame.rowconfigure(1, weight=1)
        
        Log.debug(f"{header}: Monitor tab created successfully")


    """
    Create digital display widgets for measurements.
    """
    def create_measurement_displays(self, parent):
        header = "TachometerGUI.create_measurement_displays()"
        Log.trace(f"{header}: Creating measurement displays")
        
        # Measurement variables
        self.rpm_var = tk.StringVar(value="0")
        self.filtered_rpm_var = tk.StringVar(value="0")
        self.frequency_var = tk.StringVar(value="0")
        self.filtered_freq_var = tk.StringVar(value="0")
        self.total_revs_var = tk.StringVar(value="0")
        self.pulse_interval_var = tk.StringVar(value="0")
        
        # Create display widgets
        displays = [
            ("RPM (Raw)", self.rpm_var, "cyan"),
            ("RPM (Filtered)", self.filtered_rpm_var, "cyan"),
            ("Frequency (Hz)", self.frequency_var, "cyan"),
            ("Filtered Freq (Hz)", self.filtered_freq_var, "cyan"),
            ("Total Revolutions", self.total_revs_var, "cyan"),
            ("Pulse Interval (µs)", self.pulse_interval_var, "cyan")
        ]
        
        for i, (label, var, color) in enumerate(displays):
            # Label
            ttk.Label(parent, text=label, font=('Arial', 10, 'bold')).grid(
                row=i, column=0, sticky="w", pady=2)
            
            # Value display
            value_label = tk.Label(parent, textvariable=var, 
                                 font=('Arial', 14, 'bold'),
                                 fg=color, bg='black', relief='sunken',
                                 width=15, anchor='e')
            value_label.grid(row=i, column=1, sticky="ew", padx=5, pady=2)
        
        parent.columnconfigure(1, weight=1)
        
        Log.trace(f"{header}: Measurement displays created successfully")


    """
    Create control tab for system operations.
    """
    def create_control_tab(self):
        header = "TachometerGUI.create_control_tab()"
        Log.trace(f"{header}: Creating control tab")
        
        # Control tab frame
        self.control_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.control_frame, text="🎛️ System Control")
        
        # Reset controls frame
        reset_frame = ttk.LabelFrame(self.control_frame, text="Reset Operations", padding=10)
        reset_frame.grid(row=0, column=0, sticky="ew", padx=5, pady=5)
        
        reset_buttons = [
            ("Reset All", "RESET_ALL"),
            ("Reset Filters", "RESET_FILTERS"),
            ("Reset Revolutions", "RESET_REVS"),
            ("Reset System", "RESET_SYSTEM")
        ]
        
        for i, (text, command) in enumerate(reset_buttons):
            btn = ttk.Button(reset_frame, text=text, 
                           command=lambda cmd=command: self.send_command(cmd))
            btn.grid(row=i//2, column=i%2, padx=5, pady=5, sticky="ew")
        
        reset_frame.columnconfigure(0, weight=1)
        reset_frame.columnconfigure(1, weight=1)
        
        # Filter controls frame
        filter_frame = ttk.LabelFrame(self.control_frame, text="Filter Controls", padding=10)
        filter_frame.grid(row=1, column=0, sticky="ew", padx=5, pady=5)
        
        # Filter enable/disable
        ttk.Button(filter_frame, text="Toggle Filtering", 
                  command=lambda: self.send_command("TOGGLE_FILTER")).grid(
                  row=0, column=0, columnspan=3, pady=5, sticky="ew")
        
        # Filter alpha control
        ttk.Label(filter_frame, text="Filter Alpha (0-1000):").grid(row=1, column=0, sticky="w")
        self.alpha_var = tk.IntVar(value=800)
        alpha_scale = ttk.Scale(filter_frame, from_=0, to=1000, variable=self.alpha_var,
                               orient=tk.HORIZONTAL, command=self.on_alpha_change)
        alpha_scale.grid(row=1, column=1, sticky="ew", padx=5)
        
        self.alpha_label = ttk.Label(filter_frame, text="800")
        self.alpha_label.grid(row=1, column=2, padx=5)
        
        # Window size control
        ttk.Label(filter_frame, text="Window Size (1-20):").grid(row=2, column=0, sticky="w")
        self.window_var = tk.IntVar(value=5)
        window_scale = ttk.Scale(filter_frame, from_=1, to=20, variable=self.window_var,
                                orient=tk.HORIZONTAL, command=self.on_window_change)
        window_scale.grid(row=2, column=1, sticky="ew", padx=5)
        
        self.window_label = ttk.Label(filter_frame, text="5")
        self.window_label.grid(row=2, column=2, padx=5)
        
        filter_frame.columnconfigure(1, weight=1)
        
        # Configure grid weights
        self.control_frame.columnconfigure(0, weight=1)
        
        Log.debug(f"{header}: Control tab created successfully")


    """
    Create configuration tab for system parameters.
    """
    def create_config_tab(self):
        header = "TachometerGUI.create_config_tab()"
        Log.trace(f"{header}: Creating configuration tab")
        
        # Config tab frame
        self.config_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.config_frame, text="⚙️ Configuration")
        
        # System parameters frame
        params_frame = ttk.LabelFrame(self.config_frame, text="System Parameters", padding=10)
        params_frame.grid(row=0, column=0, sticky="ew", padx=5, pady=5)
        
        # Debounce time control
        ttk.Label(params_frame, text="Debounce Time (µs):").grid(row=0, column=0, sticky="w")
        self.debounce_var = tk.IntVar(value=100)
        debounce_spin = ttk.Spinbox(params_frame, from_=0, to=1000, textvariable=self.debounce_var,
                                   width=10, command=self.on_debounce_change)
        debounce_spin.grid(row=0, column=1, padx=5, sticky="w")
        
        # Sample period control
        ttk.Label(params_frame, text="Sample Period (ms):").grid(row=1, column=0, sticky="w")
        self.period_var = tk.IntVar(value=1000)
        period_spin = ttk.Spinbox(params_frame, from_=100, to=5000, textvariable=self.period_var,
                                 width=10, command=self.on_period_change)
        period_spin.grid(row=1, column=1, padx=5, sticky="w")
        
        # Apply button
        ttk.Button(params_frame, text="Apply Changes", 
                  command=self.apply_config_changes).grid(row=2, column=0, columnspan=2, pady=10)
        
        # Current configuration display
        info_frame = ttk.LabelFrame(self.config_frame, text="Current Configuration", padding=10)
        info_frame.grid(row=1, column=0, sticky="nsew", padx=5, pady=5)
        
        self.config_text = scrolledtext.ScrolledText(info_frame, height=15, width=50)
        self.config_text.pack(fill=tk.BOTH, expand=True)
        
        # Configure grid weights
        self.config_frame.columnconfigure(0, weight=1)
        self.config_frame.rowconfigure(1, weight=1)
        
        Log.debug(f"{header}: Configuration tab created successfully")


    """
    Create logging tab for system messages.
    """
    def create_log_tab(self):
        header = "TachometerGUI.create_log_tab()"
        Log.trace(f"{header}: Creating log tab")
        
        # Log tab frame
        self.log_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.log_frame, text="📋 System Log")
        
        # Log controls frame
        log_controls = ttk.Frame(self.log_frame)
        log_controls.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(log_controls, text="Clear Log", 
                  command=self.clear_log).pack(side=tk.LEFT, padx=5)
        
        ttk.Button(log_controls, text="Save Log", 
                  command=self.save_log).pack(side=tk.LEFT, padx=5)
        
        # Auto-scroll checkbox
        self.auto_scroll_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(log_controls, text="Auto-scroll", 
                       variable=self.auto_scroll_var).pack(side=tk.RIGHT, padx=5)
        
        # Log text area
        self.log_text = scrolledtext.ScrolledText(self.log_frame, height=25, width=80)
        self.log_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Configure log text tags for colored output
        self.log_text.tag_config("INFO", foreground="blue")
        self.log_text.tag_config("WARNING", foreground="orange")
        self.log_text.tag_config("ERROR", foreground="red")
        self.log_text.tag_config("SUCCESS", foreground="green")
        
        Log.debug(f"{header}: Log tab created successfully")


    """
    Setup matplotlib charts for real-time data visualization.
    """
    def setup_plotting(self):
        header = "TachometerGUI.setup_plotting()"
        Log.trace(f"{header}: Setting up real-time charts")
               
        try:
            # Create matplotlib figure
            self.fig = Figure(figsize=(8, 6), dpi=100)
            self.fig.patch.set_facecolor('white')
            
            # Create subplots
            self.ax1 = self.fig.add_subplot(211)
            self.ax2 = self.fig.add_subplot(212)
            
            # Configure plots
            self.ax1.set_title("RPM vs Time")
            self.ax1.set_ylabel("RPM")
            self.ax1.grid(True, alpha=0.3)
            
            self.ax2.set_title("Frequency vs Time")
            self.ax2.set_xlabel("Time (s)")
            self.ax2.set_ylabel("Frequency (Hz)")
            self.ax2.grid(True, alpha=0.3)
            
            # Create plot lines
            self.rpm_line, = self.ax1.plot([], [], 'b-', label='Raw RPM', linewidth=2)
            self.filtered_rpm_line, = self.ax1.plot([], [], 'r-', label='Filtered RPM', linewidth=2)
            self.freq_line, = self.ax2.plot([], [], 'g-', label='Frequency', linewidth=2)
            
            # Add legends
            self.ax1.legend()
            self.ax2.legend()
            
            # Embed plot in tkinter
            self.canvas = FigureCanvasTkinter(self.fig, self.chart_frame)
            self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
            
            # Setup animation
            self.anim = animation.FuncAnimation(self.fig, self.update_plots, 
                                              interval=200, blit=False)
            
            Log.info(f"{header}: Chart setup completed successfully")
            
        except Exception as e:
            Log.error(f"{header}: Error setting up charts - {str(e)}")
            self.log_message(f"Chart setup error: {str(e)}", "ERROR")


    """
    Refresh available serial ports list.
    """
    def refresh_ports(self):
        header = "TachometerGUI.refresh_ports()"
        Log.trace(f"{header}: Refreshing serial ports")
        
        try:
            ports = serial.tools.list_ports.comports()
            port_list = [port.device for port in ports]
            
            self.port_combo['values'] = port_list
            if port_list:
                self.port_combo.current(0)
                Log.info(f"{header}: Found {len(port_list)} serial ports")
                self.log_message(f"Found {len(port_list)} serial ports", "INFO")
            else:
                Log.warning(f"{header}: No serial ports found")
                self.log_message("No serial ports found", "WARNING")
                
        except Exception as e:
            Log.error(f"{header}: Error refreshing ports - {str(e)}")
            self.log_message(f"Error refreshing ports: {str(e)}", "ERROR")


    """
    Toggle serial connection to Arduino.
    """
    def toggle_connection(self):
        header = "TachometerGUI.toggle_connection()"
        
        if not self.is_connected:
            # Connect
            try:
                port = self.port_combo.get()
                if not port:
                    Log.warning(f"{header}: No port selected for connection")
                    messagebox.showerror("Error", "Please select a serial port")
                    return
                
                Log.info(f"{header}: Attempting connection to {port}")
                self.serial_port = serial.Serial(port, 115200, timeout=1)
                time.sleep(2)  # Wait for Arduino reset
                
                # Start communication thread
                self.stop_threads = False
                self.serial_thread = threading.Thread(target=self.serial_communication_thread)
                self.serial_thread.daemon = True
                self.serial_thread.start()
                
                self.is_connected = True
                self.connect_btn.config(text="Disconnect")
                self.status_label.config(text="Connected")
                
                Log.info(f"{header}: Successfully connected to {port}")
                self.log_message(f"Connected to {port}", "SUCCESS")
                
                # Request initial configuration
                self.send_command("GET_CONFIG")
                
            except Exception as e:
                Log.error(f"{header}: Connection error - {str(e)}")
                self.log_message(f"Connection failed: {str(e)}", "ERROR")
                messagebox.showerror("Connection Error", str(e))
        else:
            # Disconnect
            self.disconnect()


    """
    Disconnect from Arduino.
    """
    def disconnect(self):
        header = "TachometerGUI.disconnect()"
        Log.trace(f"{header}: Disconnecting from Arduino")
        
        try:
            self.stop_threads = True
            
            if self.serial_thread and self.serial_thread.is_alive():
                self.serial_thread.join(timeout=2)
                Log.debug(f"{header}: Serial communication thread stopped")
            
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
                Log.debug(f"{header}: Serial port closed")
            
            self.is_connected = False
            self.connect_btn.config(text="Connect")
            self.status_label.config(text="Disconnected")
            
            Log.info(f"{header}: Successfully disconnected from Arduino")
            self.log_message("Disconnected from Arduino", "INFO")
            
        except Exception as e:
            Log.error(f"{header}: Disconnect error - {str(e)}")
            self.log_message(f"Disconnect error: {str(e)}", "ERROR")


    """
    Background thread for serial communication with Arduino.
    """
    def serial_communication_thread(self):
        header = "TachometerGUI.serial_communication_thread()"
        Log.trace(f"{header}: Starting serial communication thread")
        
        while not self.stop_threads and self.is_connected:
            try:
                # Send pending commands
                while not self.command_queue.empty():
                    command = self.command_queue.get_nowait()
                    self.serial_port.write(f"CMD:{command}\n".encode())
                    self.serial_port.flush()
                    Log.trace(f"{header}: Sent command: {command}")
                
                # Read incoming data
                if self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode().strip()
                    if line:
                        Log.trace(f"{header}: Received: {line}")
                        self.process_incoming_data(line)
                
                time.sleep(0.01)  # Small delay to prevent CPU overload
                
            except Exception as e:
                Log.error(f"{header}: Communication error - {str(e)}")
                self.log_message(f"Communication error: {str(e)}", "ERROR")
                break
        
        Log.debug(f"{header}: Serial communication thread stopped")


    """
    Process data received from Arduino.
    """
    def process_incoming_data(self, line):
        header = "TachometerGUI.process_incoming_data()"
        
        try:
            if line.startswith("DATA:"):
                # Parse measurement data
                data_str = line[5:]  # Remove "DATA:" prefix
                values = data_str.split(',')
                
                if len(values) >= 8:
                    self.current_data = {
                        'frequency': int(values[0]),
                        'rpm': int(values[1]),
                        'filtered_freq': int(values[2]),
                        'filtered_rpm': int(values[3]),
                        'total_revs': int(values[4]),
                        'raw_pulses': int(values[5]),
                        'pulse_interval': int(values[6]),
                        'timestamp': int(values[7])
                    }
                    
                    # Add to data queue for GUI update
                    self.data_queue.put(('DATA', self.current_data))
                    Log.trace(f"{header}: Parsed measurement data successfully")
                    
            elif line.startswith("CONFIG:"):
                # Parse configuration data
                config_str = line[7:]  # Remove "CONFIG:" prefix
                values = config_str.split(',')
                
                if len(values) >= 8:
                    self.config = {
                        'ir_pin': int(values[0]),
                        'sample_period': int(values[1]),
                        'debounce_time': int(values[2]),
                        'pulses_per_rev': int(values[3]),
                        'timer_number': int(values[4]),
                        'filtering_enabled': bool(int(values[5])),
                        'filter_alpha': int(values[6]),
                        'window_size': int(values[7])
                    }
                    
                    self.data_queue.put(('CONFIG', self.config))
                    Log.debug(f"{header}: Configuration data received and parsed")
                    
            elif line.startswith("RESP:"):
                # Handle command responses
                response = line[5:]  # Remove "RESP:" prefix
                self.data_queue.put(('RESPONSE', response))
                Log.debug(f"{header}: Command response received: {response}")
                
            elif line.startswith("TACH:"):
                # Handle system messages
                message = line[5:]  # Remove "TACH:" prefix
                self.data_queue.put(('SYSTEM', message))
                Log.debug(f"{header}: System message received: {message}")
                
            else:
                # Unknown message format
                self.data_queue.put(('RAW', line))
                Log.warning(f"{header}: Unknown message format: {line}")
                
        except Exception as e:
            Log.error(f"{header}: Error parsing data '{line}' - {str(e)}")
            self.log_message(f"Data parsing error: {str(e)}", "ERROR")


    """
    Send command to Arduino.
    """
    def send_command(self, command):
        header = f"TachometerGUI.send_command(command={command})"
        Log.trace(f"{header}: Sending command to Arduino")
        
        if self.is_connected:
            self.command_queue.put(command)
            Log.info(f"{header}: Command queued: {command}")
            self.log_message(f"Command sent: {command}", "INFO")
        else:
            Log.warning(f"{header}: Cannot send command - not connected")
            self.log_message("Cannot send command: Not connected", "WARNING")
            messagebox.showwarning("Warning", "Not connected to Arduino")


    """
    Timer function to update GUI with new data.
    """
    def update_gui_timer(self):
        try:
            # Process all pending data updates
            while not self.data_queue.empty():
                data_type, data = self.data_queue.get_nowait()
                
                if data_type == 'DATA':
                    self.update_measurements(data)
                    self.add_data_to_plot(data)
                    
                elif data_type == 'CONFIG':
                    self.update_configuration_display(data)
                    
                elif data_type == 'RESPONSE':
                    self.handle_command_response(data)
                    
                elif data_type == 'SYSTEM':
                    self.handle_system_message(data)
                    
                elif data_type == 'RAW':
                    self.log_message(f"Raw: {data}", "INFO")
                    
        except Exception as e:
            Log.error(f"GUI update error: {str(e)}")
        
        # Schedule next update
        self.root.after(50, self.update_gui_timer)


    """
    Format to BR numbers.
    """
    def format_br_number(self, number):
        """Formatar número no padrão brasileiro."""
        if number == 0:
            return "0"
        return f"{number:,}".replace(",", ".")


    """
    Update measurement displays with new data.
    """
    def update_measurements(self, data):
        header = "TachometerGUI.update_measurements()"
        
        try:
            self.rpm_var.set(format_rpm_br(data['rpm']))
            self.filtered_rpm_var.set(format_rpm_br(data['filtered_rpm']))
            self.frequency_var.set(format_frequency_br(data['frequency']))
            self.filtered_freq_var.set(format_frequency_br(data['filtered_freq']))
            self.total_revs_var.set(format_revolutions_br(data['total_revs']))
            self.pulse_interval_var.set(format_time_br(data['pulse_interval']))
            
            Log.trace(f"{header}: Measurement displays updated successfully")
            
        except Exception as e:
            Log.error(f"{header}: Error updating measurements - {str(e)}")


    """
    Add new data point to plotting arrays.
    """
    def add_data_to_plot(self, data):
        header = "TachometerGUI.add_data_to_plot()"
        
        try:
            current_time = time.time()
            
            self.time_data.append(current_time)
            self.rpm_data.append(data['rpm'])
            self.filtered_rpm_data.append(data['filtered_rpm'])
            self.frequency_data.append(data['frequency'])
            
            Log.trace(f"{header}: Plot data point added successfully")
            
        except Exception as e:
            Log.error(f"{header}: Error adding plot data - {str(e)}")


    """
    Update matplotlib plots with new data.
    """
    def update_plots(self, frame):
        header = "TachometerGUI.update_plots()"
        
        if len(self.time_data) < 2:
            return
        
        try:
            # Calculate relative time
            if len(self.time_data) > 0:
                start_time = self.time_data[0]
                rel_time = [t - start_time for t in self.time_data]
                
                # Update plot data
                self.rpm_line.set_data(rel_time, list(self.rpm_data))
                self.filtered_rpm_line.set_data(rel_time, list(self.filtered_rpm_data))
                self.freq_line.set_data(rel_time, list(self.frequency_data))
                
                # Auto-scale axes
                self.ax1.relim()
                self.ax1.autoscale_view()
                self.ax2.relim()
                self.ax2.autoscale_view()
                
                Log.trace(f"{header}: Plots updated successfully")
                
        except Exception as e:
            Log.error(f"{header}: Error updating plots - {str(e)}")


    """
    Handle command responses from Arduino.
    """
    def handle_command_response(self, response):
        header = "TachometerGUI.handle_command_response()"
        Log.trace(f"{header}: Processing command response: {response}")
        self.log_message(f"Response: {response}", "SUCCESS")


    """
    Handle system messages from Arduino.
    """
    def handle_system_message(self, message):
        header = "TachometerGUI.handle_system_message()"
        Log.trace(f"{header}: Processing system message: {message}")
        
        if message == "STARTUP":
            Log.info(f"{header}: Arduino system starting up")
            self.log_message("Arduino system starting up", "INFO")
        elif message == "INIT_OK":
            Log.info(f"{header}: Arduino initialization successful")
            self.log_message("Arduino initialization successful", "SUCCESS")
        elif message == "INIT_ERROR":
            Log.error(f"{header}: Arduino initialization failed")
            self.log_message("Arduino initialization failed", "ERROR")
        else:
            Log.info(f"{header}: System message: {message}")
            self.log_message(f"System: {message}", "INFO")


    """
    Update configuration display with current settings.
    """
    def update_configuration_display(self, config):
        header = "TachometerGUI.update_configuration_display()"
        Log.trace(f"{header}: Updating configuration display")
        
        try:
            config_text = "Current System Configuration:\n"
            config_text += "=" * 40 + "\n"
            config_text += f"IR Sensor Pin: {config['ir_pin']}\n"
            config_text += f"Sample Period: {config['sample_period']} ms\n"
            config_text += f"Debounce Time: {config['debounce_time']} µs\n"
            config_text += f"Pulses per Revolution: {config['pulses_per_rev']}\n"
            config_text += f"Timer Number: {config['timer_number']}\n"
            config_text += f"Filtering Enabled: {config['filtering_enabled']}\n"
            config_text += f"Filter Alpha: {config['filter_alpha']}\n"
            config_text += f"Window Size: {config['window_size']}\n"
            config_text += f"Last Updated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
            
            self.config_text.delete(1.0, tk.END)
            self.config_text.insert(1.0, config_text)
            
            # Update control widgets
            self.alpha_var.set(config['filter_alpha'])
            self.window_var.set(config['window_size'])
            self.debounce_var.set(config['debounce_time'])
            self.period_var.set(config['sample_period'])
            
            self.alpha_label.config(text=str(config['filter_alpha']))
            self.window_label.config(text=str(config['window_size']))
            
            Log.debug(f"{header}: Configuration display updated successfully")
            
        except Exception as e:
            Log.error(f"{header}: Error updating configuration display - {str(e)}")


    """
    Handle alpha slider change event.
    """
    def on_alpha_change(self, value):
        header = "TachometerGUI.on_alpha_change()"
        Log.trace(f"{header}: Processing alpha filter change")
        alpha_val = int(float(value))
        self.alpha_label.config(text=str(alpha_val))
        self.send_command(f"SET_ALPHA:{alpha_val}")
        Log.debug(f"{header}: Alpha filter changed to {alpha_val}")


    """
    Handle window size slider change event.
    """
    def on_window_change(self, value):
        header = "TachometerGUI.on_window_change()"
        Log.trace(f"{header}: Processing window size change")
        window_val = int(float(value))
        self.window_label.config(text=str(window_val))
        self.send_command(f"SET_WINDOW:{window_val}")
        Log.debug(f"{header}: Window size changed to {window_val}")


    """
    Handle debounce time change event.
    """
    def on_debounce_change(self):
        header = "TachometerGUI.on_debounce_change()"
        Log.trace(f"{header}: Processing debounce change event")
        debounce_val = self.debounce_var.get()
        self.send_command(f"SET_DEBOUNCE:{debounce_val}")
        Log.debug(f"{header}: Debounce time changed to {debounce_val} µs")


    """
    Handle sample period change event.
    """
    def on_period_change(self):
        header = "TachometerGUI.on_period_change()"
        Log.trace(f"{header}: Processing sample period change event")
        period_val = self.period_var.get()
        self.send_command(f"SET_PERIOD:{period_val}")
        Log.debug(f"{header}: Sample period changed to {period_val} ms")


    """
    Apply configuration changes to Arduino.
    """
    def apply_config_changes(self):
        header = "TachometerGUI.apply_config_changes()"
        Log.trace(f"{header}: Applying configuration changes")
        
        try:
            debounce_val = self.debounce_var.get()
            period_val = self.period_var.get()
            
            self.send_command(f"SET_DEBOUNCE:{debounce_val}")
            self.send_command(f"SET_PERIOD:{period_val}")
            
            Log.info(f"{header}: Configuration changes applied successfully")
            self.log_message("Configuration changes applied", "SUCCESS")
            
        except Exception as e:
            Log.error(f"{header}: Error applying changes - {str(e)}")
            self.log_message(f"Error applying changes: {str(e)}", "ERROR")


    """
    Add message to system log with timestamp.
    """
    def log_message(self, message, level="INFO"):
        header = "TachometerGUI.log_message()"
        
        try:
            timestamp = datetime.now().strftime("%H:%M:%S")
            log_entry = f"[{timestamp}] {level}: {message}\n"
            
            self.log_text.insert(tk.END, log_entry, level)
            
            if self.auto_scroll_var.get():
                self.log_text.see(tk.END)
                
            Log.trace(f"{header}: GUI log message added: {message}")
                
        except Exception as e:
            Log.error(f"{header}: Error logging GUI message - {str(e)}")


    """
    Clear the system log.
    """
    def clear_log(self):
        header = "TachometerGUI.clear_log()"
        Log.trace(f"{header}: Clearing system log")
        
        self.log_text.delete(1.0, tk.END)
        self.log_message("Log cleared", "INFO")


    """
    Save system log to file.
    """
    def save_log(self):
        header = "TachometerGUI.save_log()"
        Log.trace(f"{header}: Saving system log to file")
        
        try:
            filename = filedialog.asksaveasfilename(
                defaultextension=".txt",
                filetypes=[("Text files", "*.txt"), ("All files", "*.*")],
                title="Save System Log"
            )
            
            if filename:
                with open(filename, 'w', encoding='utf-8') as f:
                    log_content = self.log_text.get(1.0, tk.END)
                    f.write(log_content)
                
                Log.info(f"{header}: System log saved to {filename}")
                self.log_message(f"Log saved to {filename}", "SUCCESS")
                
        except Exception as e:
            Log.error(f"{header}: Error saving log - {str(e)}")
            self.log_message(f"Error saving log: {str(e)}", "ERROR")
            messagebox.showerror("Save Error", str(e))


    """
    Handle application closing event.
    """
    def on_closing(self):
        header = "TachometerGUI.on_closing()"
        Log.trace(f"{header}: Application closing initiated")
        
        if self.is_connected:
            Log.debug(f"{header}: Disconnecting before closing")
            self.disconnect()
        
        Log.info(f"{header}: Closing log file")
        Log.close_log_file()
        
        self.root.destroy()
        Log.info(f"{header}: Application closed successfully")


    """
    Start the GUI application main loop.
    """
    def run(self):
        header = "TachometerGUI.run()"
        Log.trace(f"{header}: Starting GUI main loop")
        
        # Initialize port list
        self.refresh_ports()
        
        # Start main loop
        Log.debug(f"{header}: Entering tkinter main loop")
        self.root.mainloop()


"""
Main application entry point.
"""
def main():
    header = "main()"
    
    # Initialize logging system early
    Log.set_level(LogLevel.INFO)
    Log.set_console_output(True)
    
    Log.info(f"{header}: Starting Industrial Tachometer GUI application")
    
    try:
        app = TachometerGUI()
        app.run()
        
    except Exception as e:
        Log.critical(f"{header}: Fatal application error - {str(e)}")
        messagebox.showerror("Fatal Error", f"Application failed to start: {str(e)}")
    
    finally:
        Log.info(f"{header}: Application terminated")
        Log.close_log_file()


if __name__ == "__main__":
    main()
    