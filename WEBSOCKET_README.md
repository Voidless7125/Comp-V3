# VEX WebSocket Client Documentation

This document describes the Python WebSocket client implementation for receiving telemetry data from VEX robots via the VEX Extension's WebSocket server.

## Overview

The WebSocket client connects to the VEX Extension server running on port 7071 and receives real-time telemetry data from the robot, including:

- Motor temperatures
- Motor RPM (commanded and actual)
- Motor position and current
- Battery voltage
- Inertial sensor data (gyro axes)
- System logs and warnings

## Installation

### Prerequisites

- Python 3.7 or higher
- VEX Extension installed in VS Code
- USB connection to VEX Brain or Controller

### Dependencies

Install the required Python packages:

```bash
pip3 install -r requirements.txt
```

Or manually:

```bash
pip3 install websocket-client
```

## Usage

### Basic Usage

```bash
# Connect to default localhost:7071
python3 websocket_client.py

# Connect to specific host/port
python3 websocket_client.py --host 192.168.1.100 --port 7071

# Save data to CSV file
python3 websocket_client.py --save-to-file

# Enable verbose logging
python3 websocket_client.py --verbose
```

### Command Line Options

- `--host HOST`: WebSocket server host (default: localhost)
- `--port PORT`: WebSocket server port (default: 7071)
- `--save-to-file`: Save received data to timestamped CSV file
- `--verbose`: Enable verbose logging

## Data Format

### Raw WebSocket Messages

The VEX Extension forwards data from the robot's printf() statements. Example messages:

```
[Info] > Time: 15.234 > Module: motorMonitor > 
 | LeftTemp: 45°
 | RightTemp: 48°
 | RearLeftTemp: 42°
 | RearRightTemp: 46°
 | Battery Voltage: 12.5V

[Info] > Time: 15.250 > Module: motorMonitor > 
X Axis: 2.5
Y Axis: -1.2
Z Axis: 180.0

[Info] > Time: 15.265 > Module: motorMonitor > 
 | FLM: CmdRPM: 150 | ActRPM: 145 | Pos: 2.75rev | Current: 1.2A
```

### Parsed Data Structure

The client parses raw messages into structured data:

```python
{
    'timestamp': '2024-01-01T12:00:00.123456',
    'raw_message': '[Info] > Time: 15.234 > Module: motorMonitor > ...',
    'module': 'motorMonitor',
    'log_level': 'Info',
    'time_sec': 15.234,
    'motor_temps': {
        'left': 45,
        'right': 48,
        'rearleft': 42,
        'rearright': 46
    },
    'battery_voltage': 12.5,
    'gyro_axes': {
        'x': 2.5,
        'y': -1.2,
        'z': 180.0
    },
    'motor_details': {
        'flm': {
            'commanded_rpm': 150,
            'actual_rpm': 145,
            'position_rev': 2.75,
            'current_amps': 1.2
        }
    }
}
```

## CSV Output Format

When `--save-to-file` is enabled, data is saved to a timestamped CSV file with columns:

- `timestamp`: ISO format timestamp
- `module`: Source module (e.g., motorMonitor)
- `log_level`: Log level (Info, Warn, Error, etc.)
- `time_sec`: Robot time in seconds
- `left_temp`, `right_temp`, `rear_left_temp`, `rear_right_temp`: Motor temperatures
- `battery_voltage`: Battery voltage in volts
- `x_axis`, `y_axis`, `z_axis`: Gyro axes in degrees
- `motor_name`: Motor identifier for detailed data
- `commanded_rpm`: Commanded motor RPM
- `actual_rpm`: Actual motor RPM
- `position_rev`: Motor position in revolutions
- `current_amps`: Motor current in amperes
- `raw_message`: Original WebSocket message

## VEX Robot Code Changes

The C++ robot code has been enhanced to output more detailed telemetry data. The `motorMonitor()` function now includes:

### Enhanced Motor Data Output

```cpp
// Log detailed motor data including RPM, current, and position
std::array motors = {&frontLeftMotor, &frontRightMotor, &rearLeftMotor, &rearRightMotor};
for (size_t i = 0; i < motors.size(); ++i)
{
    if (motors[i]->installed())
    {
        int commandedRpm = vexMotorVelocityGet(motors[i]->index());
        int actualRpm = motors[i]->velocity(vex::velocityUnits::rpm);
        double position = motors[i]->position(vex::rotationUnits::rev);
        double current = motors[i]->current();
        
        std::string motorDetailStr = std::format(
            "\n | {}: CmdRPM: {} | ActRPM: {} | Pos: {:.2f}rev | Current: {:.2f}A",
            motorNames[i], commandedRpm, actualRpm, position, current);
        logHandler("motorMonitor", motorDetailStr, Log::Level::Info);
    }
}
```

### Data Types Available

- **Motor Temperature**: Temperature in Celsius
- **Commanded RPM**: Target motor speed from VEX C API
- **Actual RPM**: Real motor speed from encoders
- **Position**: Motor encoder position in revolutions
- **Current**: Motor current draw in amperes
- **Battery Voltage**: Brain battery voltage
- **Inertial Data**: Gyro pitch, roll, yaw in degrees

## Testing

Run the test script to validate parsing functionality:

```bash
python3 test_websocket_client.py
```

This tests the parser with simulated VEX robot messages without requiring a live connection.

## Error Handling

The client includes robust error handling:

- **Connection failures**: Automatic reconnection attempts every 5 seconds
- **Parse errors**: Logged but don't interrupt the connection
- **File I/O errors**: Graceful fallback when CSV writing fails
- **Keyboard interrupt**: Clean shutdown with statistics display

## Example Output

```
VEX WebSocket Client for Motor Monitoring
==================================================
Connecting to: ws://localhost:7071/vexrobotics.vexcode/device
Save to file: Yes
==================================================
Press Ctrl+C to stop

Connected to VEX Extension WebSocket server at ws://localhost:7071
Waiting for telemetry data...

==================================================
MOTOR TELEMETRY - Time: 15.234s
==================================================
Motor Temperatures:
  LEFT: 45°C ✅ OK
  RIGHT: 48°C ✅ OK
  REARLEFT: 42°C ✅ OK
  REARRIGHT: 46°C ✅ OK
Battery: 12.5V ✅ OK
Gyro Axes:
  X: 2.50°
  Y: -1.20°
  Z: 180.00°
Motor Details:
  FLM:
    RPM: 150 cmd / 145 actual
    Position: 2.75 rev
    Current: 1.20A
==================================================
```

## Troubleshooting

### Connection Issues

1. Ensure VEX Extension is installed and running in VS Code
2. Verify robot is connected via USB
3. Check that robot is selected in VEX Extension
4. Confirm port 7071 is not blocked by firewall

### No Data Received

1. Verify robot code is running and using `printf()` or `logHandler()`
2. Check that robot is in enabled mode (competition or test)
3. Ensure motor monitoring is active

### Parse Errors

The client logs parse errors but continues operation. Check:
1. Message format matches expected patterns
2. Robot code outputs data in expected format
3. No special characters interfere with parsing

## Integration Examples

### Real-time Dashboard

```python
from websocket_client import VEXWebSocketClient
import matplotlib.pyplot as plt
from collections import deque

class RealTimeDashboard:
    def __init__(self):
        self.temps = deque(maxlen=100)
        self.times = deque(maxlen=100)
        
    def update_plot(self, data):
        if data['motor_temps']:
            avg_temp = sum(data['motor_temps'].values()) / len(data['motor_temps'])
            self.temps.append(avg_temp)
            self.times.append(data.get('time_sec', 0))
            
            plt.plot(self.times, self.temps)
            plt.pause(0.01)
```

### Data Analysis

```python
import pandas as pd

# Load CSV data
df = pd.read_csv('vex_telemetry_20240101_120000.csv')

# Analyze motor performance
motor_data = df[df['motor_name'].notna()]
print(motor_data.groupby('motor_name')[['actual_rpm', 'current_amps']].mean())

# Plot temperature trends
import matplotlib.pyplot as plt
df.plot(x='time_sec', y=['left_temp', 'right_temp'], kind='line')
plt.show()
```

## License

This code is provided as-is for educational and development purposes with VEX robotics systems.