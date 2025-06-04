#!/usr/bin/env python3
"""
VEX WebSocket Client for Motor Monitoring

This client connects to the VEX Extension's WebSocket server to receive
real-time data from the robot, including motor temperatures, RPM, battery
voltage, gyro data, and other telemetry information.

Usage:
    python3 websocket_client.py [--save-to-file] [--host localhost] [--port 7071]
"""

import websocket
import json
import threading
import time
import argparse
import re
import csv
from datetime import datetime
from typing import Dict, List, Optional, Any
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)


class VEXWebSocketClient:
    """WebSocket client for receiving VEX robot telemetry data."""
    
    def __init__(self, host: str = "localhost", port: int = 7071, save_to_file: bool = False):
        """
        Initialize the VEX WebSocket client.
        
        Args:
            host: WebSocket server host (default: localhost)
            port: WebSocket server port (default: 7071)
            save_to_file: Whether to save received data to file
        """
        self.host = host
        self.port = port
        self.save_to_file = save_to_file
        self.ws = None
        self.is_connected = False
        self.data_log = []
        self.csv_file = None
        self.csv_writer = None
        
        # Data parsing patterns
        self.motor_temp_pattern = re.compile(r'(\w+)Temp: (\d+)°')
        self.battery_pattern = re.compile(r'Battery Voltage: ([\d.]+)V')
        self.axis_pattern = re.compile(r'([XYZ]) Axis: ([-\d.]+)')
        self.motor_display_pattern = re.compile(r'(\w+): (\d+)°')
        self.motor_detail_pattern = re.compile(r'(\w+): CmdRPM: (-?\d+) \| ActRPM: (-?\d+) \| Pos: ([-\d.]+)rev \| Current: ([\d.]+)A')
        
        # Initialize CSV file if saving is enabled
        if self.save_to_file:
            self._init_csv_file()
    
    def _init_csv_file(self):
        """Initialize CSV file for data logging."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"vex_telemetry_{timestamp}.csv"
        
        try:
            self.csv_file = open(filename, 'w', newline='')
            fieldnames = [
                'timestamp', 'module', 'log_level', 'time_sec',
                'left_temp', 'right_temp', 'rear_left_temp', 'rear_right_temp',
                'battery_voltage', 'x_axis', 'y_axis', 'z_axis',
                'motor_name', 'commanded_rpm', 'actual_rpm', 'position_rev', 'current_amps',
                'raw_message'
            ]
            self.csv_writer = csv.DictWriter(self.csv_file, fieldnames=fieldnames)
            self.csv_writer.writeheader()
            logger.info(f"CSV logging initialized: {filename}")
        except Exception as e:
            logger.error(f"Failed to initialize CSV file: {e}")
            self.save_to_file = False
    
    def parse_motor_data(self, message: str) -> Dict[str, Any]:
        """
        Parse motor telemetry data from WebSocket message.
        
        Args:
            message: Raw message from WebSocket
            
        Returns:
            Dictionary containing parsed telemetry data
        """
        data = {
            'timestamp': datetime.now().isoformat(),
            'raw_message': message,
            'motor_temps': {},
            'battery_voltage': None,
            'gyro_axes': {},
            'motor_details': {},
            'module': None,
            'log_level': None,
            'time_sec': None
        }
        
        # Parse log header (log level, time, module)
        log_header_match = re.search(r'\[(\w+)\] > Time: ([\d.]+) > Module: (\w+)', message)
        if log_header_match:
            data['log_level'] = log_header_match.group(1)
            data['time_sec'] = float(log_header_match.group(2))
            data['module'] = log_header_match.group(3)
        
        # Parse motor temperatures
        for match in self.motor_temp_pattern.finditer(message):
            motor_name = match.group(1).lower()
            temperature = int(match.group(2))
            data['motor_temps'][motor_name] = temperature
        
        # Parse battery voltage
        battery_match = self.battery_pattern.search(message)
        if battery_match:
            data['battery_voltage'] = float(battery_match.group(1))
        
        # Parse gyro axes
        for match in self.axis_pattern.finditer(message):
            axis = match.group(1).lower()
            value = float(match.group(2))
            data['gyro_axes'][axis] = value
        
        # Parse detailed motor data (RPM, position, current)
        motor_detail_match = self.motor_detail_pattern.search(message)
        if motor_detail_match:
            motor_name = motor_detail_match.group(1).lower()
            data['motor_details'][motor_name] = {
                'commanded_rpm': int(motor_detail_match.group(2)),
                'actual_rpm': int(motor_detail_match.group(3)),
                'position_rev': float(motor_detail_match.group(4)),
                'current_amps': float(motor_detail_match.group(5))
            }
        
        return data
    
    def on_message(self, ws, message):
        """Handle incoming WebSocket messages."""
        try:
            logger.info(f"Received: {message}")
            
            # Parse the message
            parsed_data = self.parse_motor_data(message)
            
            # Add to data log
            self.data_log.append(parsed_data)
            
            # Display parsed information
            self._display_telemetry(parsed_data)
            
            # Save to CSV if enabled
            if self.save_to_file and self.csv_writer:
                self._save_to_csv(parsed_data)
                
        except Exception as e:
            logger.error(f"Error processing message: {e}")
    
    def _display_telemetry(self, data: Dict[str, Any]):
        """Display telemetry data in a formatted way."""
        if data['module'] == 'motorMonitor':
            print("\n" + "="*50)
            print(f"MOTOR TELEMETRY - Time: {data.get('time_sec', 'N/A')}s")
            print("="*50)
            
            # Motor temperatures
            if data['motor_temps']:
                print("Motor Temperatures:")
                for motor, temp in data['motor_temps'].items():
                    status = "⚠️  HOT!" if temp >= 55 else "✅ OK"
                    print(f"  {motor.upper()}: {temp}°C {status}")
            
            # Battery voltage
            if data['battery_voltage']:
                battery_status = "⚠️  LOW!" if data['battery_voltage'] < 12 else "✅ OK"
                print(f"Battery: {data['battery_voltage']}V {battery_status}")
            
            # Gyro data
            if data['gyro_axes']:
                print("Gyro Axes:")
                for axis, value in data['gyro_axes'].items():
                    print(f"  {axis.upper()}: {value:.2f}°")
            
            # Motor details (RPM, position, current)
            if data['motor_details']:
                print("Motor Details:")
                for motor, details in data['motor_details'].items():
                    print(f"  {motor.upper()}:")
                    print(f"    RPM: {details['commanded_rpm']} cmd / {details['actual_rpm']} actual")
                    print(f"    Position: {details['position_rev']:.2f} rev")
                    print(f"    Current: {details['current_amps']:.2f}A")
            
            print("="*50)
    
    def _save_to_csv(self, data: Dict[str, Any]):
        """Save parsed data to CSV file."""
        try:
            # Handle motor details - if present, save each motor as a separate row
            if data['motor_details']:
                for motor_name, details in data['motor_details'].items():
                    row = {
                        'timestamp': data['timestamp'],
                        'module': data.get('module'),
                        'log_level': data.get('log_level'),
                        'time_sec': data.get('time_sec'),
                        'left_temp': data['motor_temps'].get('left'),
                        'right_temp': data['motor_temps'].get('right'),
                        'rear_left_temp': data['motor_temps'].get('rearleft'),
                        'rear_right_temp': data['motor_temps'].get('rearright'),
                        'battery_voltage': data.get('battery_voltage'),
                        'x_axis': data['gyro_axes'].get('x'),
                        'y_axis': data['gyro_axes'].get('y'),
                        'z_axis': data['gyro_axes'].get('z'),
                        'motor_name': motor_name,
                        'commanded_rpm': details['commanded_rpm'],
                        'actual_rpm': details['actual_rpm'],
                        'position_rev': details['position_rev'],
                        'current_amps': details['current_amps'],
                        'raw_message': data['raw_message']
                    }
                    self.csv_writer.writerow(row)
            else:
                # Save regular data without motor details
                row = {
                    'timestamp': data['timestamp'],
                    'module': data.get('module'),
                    'log_level': data.get('log_level'),
                    'time_sec': data.get('time_sec'),
                    'left_temp': data['motor_temps'].get('left'),
                    'right_temp': data['motor_temps'].get('right'),
                    'rear_left_temp': data['motor_temps'].get('rearleft'),
                    'rear_right_temp': data['motor_temps'].get('rearright'),
                    'battery_voltage': data.get('battery_voltage'),
                    'x_axis': data['gyro_axes'].get('x'),
                    'y_axis': data['gyro_axes'].get('y'),
                    'z_axis': data['gyro_axes'].get('z'),
                    'motor_name': None,
                    'commanded_rpm': None,
                    'actual_rpm': None,
                    'position_rev': None,
                    'current_amps': None,
                    'raw_message': data['raw_message']
                }
                self.csv_writer.writerow(row)
            
            self.csv_file.flush()  # Ensure data is written immediately
        except Exception as e:
            logger.error(f"Error saving to CSV: {e}")
    
    def on_error(self, ws, error):
        """Handle WebSocket errors."""
        logger.error(f"WebSocket error: {error}")
    
    def on_close(self, ws, close_status_code, close_msg):
        """Handle WebSocket connection close."""
        self.is_connected = False
        logger.info("WebSocket connection closed")
        
        if self.csv_file:
            self.csv_file.close()
            logger.info("CSV file closed")
    
    def on_open(self, ws):
        """Handle WebSocket connection open."""
        self.is_connected = True
        logger.info("WebSocket connection established")
        print(f"Connected to VEX Extension WebSocket server at ws://{self.host}:{self.port}")
        print("Waiting for telemetry data...")
    
    def connect(self):
        """Connect to the VEX WebSocket server."""
        url = f"ws://{self.host}:{self.port}/vexrobotics.vexcode/device"
        logger.info(f"Connecting to {url}")
        
        self.ws = websocket.WebSocketApp(
            url,
            on_message=self.on_message,
            on_error=self.on_error,
            on_close=self.on_close,
            on_open=self.on_open
        )
        
        # Run forever with reconnection logic
        while True:
            try:
                self.ws.run_forever()
            except KeyboardInterrupt:
                logger.info("Interrupted by user")
                break
            except Exception as e:
                logger.error(f"Connection error: {e}")
                logger.info("Attempting to reconnect in 5 seconds...")
                time.sleep(5)
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get statistics about received data."""
        if not self.data_log:
            return {"message": "No data received yet"}
        
        stats = {
            "total_messages": len(self.data_log),
            "time_range": {
                "start": self.data_log[0]['timestamp'],
                "end": self.data_log[-1]['timestamp']
            },
            "modules": {},
            "temperature_stats": {},
            "battery_readings": []
        }
        
        # Analyze data
        for entry in self.data_log:
            # Module statistics
            module = entry.get('module', 'unknown')
            stats["modules"][module] = stats["modules"].get(module, 0) + 1
            
            # Temperature statistics
            for motor, temp in entry['motor_temps'].items():
                if motor not in stats["temperature_stats"]:
                    stats["temperature_stats"][motor] = {"min": temp, "max": temp, "readings": []}
                stats["temperature_stats"][motor]["min"] = min(stats["temperature_stats"][motor]["min"], temp)
                stats["temperature_stats"][motor]["max"] = max(stats["temperature_stats"][motor]["max"], temp)
                stats["temperature_stats"][motor]["readings"].append(temp)
            
            # Battery readings
            if entry.get('battery_voltage'):
                stats["battery_readings"].append(entry['battery_voltage'])
        
        return stats


def main():
    """Main function to run the WebSocket client."""
    parser = argparse.ArgumentParser(description="VEX WebSocket Client for Motor Monitoring")
    parser.add_argument("--host", default="localhost", help="WebSocket server host (default: localhost)")
    parser.add_argument("--port", type=int, default=7071, help="WebSocket server port (default: 7071)")
    parser.add_argument("--save-to-file", action="store_true", help="Save received data to CSV file")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose logging")
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    print("VEX WebSocket Client for Motor Monitoring")
    print("="*50)
    print(f"Connecting to: ws://{args.host}:{args.port}/vexrobotics.vexcode/device")
    print(f"Save to file: {'Yes' if args.save_to_file else 'No'}")
    print("="*50)
    print("Press Ctrl+C to stop")
    print()
    
    # Create and start the client
    client = VEXWebSocketClient(host=args.host, port=args.port, save_to_file=args.save_to_file)
    
    try:
        client.connect()
    except KeyboardInterrupt:
        print("\nShutting down...")
        if client.data_log:
            print(f"Received {len(client.data_log)} messages total")
            stats = client.get_statistics()
            print("Session Statistics:")
            print(json.dumps(stats, indent=2, default=str))


if __name__ == "__main__":
    main()