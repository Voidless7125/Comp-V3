#!/usr/bin/env python3
"""
Test script for the VEX WebSocket Client

This script tests the parsing functionality of the WebSocket client
without requiring an actual WebSocket connection.
"""

import sys
from websocket_client import VEXWebSocketClient

def test_motor_data_parsing():
    """Test parsing of motor telemetry data."""
    
    # Create a client instance for testing
    client = VEXWebSocketClient(save_to_file=False)
    
    # Test messages similar to what the VEX robot would send
    test_messages = [
        "[Info] > Time: 15.234 > Module: motorMonitor > \n | LeftTemp: 45°\n | RightTemp: 48°\n | RearLeftTemp: 42°\n | RearRightTemp: 46°\n | Battery Voltage: 12.5V\n",
        "[Info] > Time: 15.250 > Module: motorMonitor > \nX Axis: 2.5\nY Axis: -1.2\nZ Axis: 180.0",
        "[Info] > Time: 15.265 > Module: motorMonitor > \n | FLM: CmdRPM: 150 | ActRPM: 145 | Pos: 2.75rev | Current: 1.2A",
        "[Info] > Time: 15.280 > Module: motorMonitor > \n | FRM: CmdRPM: 150 | ActRPM: 148 | Pos: 2.80rev | Current: 1.1A",
        "[Warn] > Time: 25.500 > Module: motorMonitor > FLM overheat: 56°"
    ]
    
    print("Testing VEX WebSocket Client Data Parsing")
    print("=" * 50)
    
    for i, message in enumerate(test_messages, 1):
        print(f"\nTest {i}: Parsing message")
        print(f"Raw: {repr(message)}")
        
        parsed_data = client.parse_motor_data(message)
        
        print("Parsed data:")
        print(f"  Module: {parsed_data.get('module')}")
        print(f"  Log Level: {parsed_data.get('log_level')}")
        print(f"  Time: {parsed_data.get('time_sec')}s")
        print(f"  Motor Temps: {parsed_data['motor_temps']}")
        print(f"  Battery: {parsed_data.get('battery_voltage')}V")
        print(f"  Gyro: {parsed_data['gyro_axes']}")
        print(f"  Motor Details: {parsed_data['motor_details']}")
        print("-" * 30)
    
    print("\nTest completed successfully!")
    return True

def test_statistics():
    """Test statistics generation."""
    client = VEXWebSocketClient(save_to_file=False)
    
    # Simulate receiving some data with proper structure
    test_data = [
        {
            "timestamp": "2024-01-01T12:00:00",
            "module": "motorMonitor", 
            "motor_temps": {"left": 45, "right": 48}, 
            "battery_voltage": 12.5
        },
        {
            "timestamp": "2024-01-01T12:00:01",
            "module": "motorMonitor", 
            "motor_temps": {"left": 46, "right": 49}, 
            "battery_voltage": 12.4
        },
        {
            "timestamp": "2024-01-01T12:00:02",
            "module": "motorMonitor", 
            "motor_temps": {"left": 47, "right": 50}, 
            "battery_voltage": 12.3
        },
    ]
    
    client.data_log = test_data
    stats = client.get_statistics()
    
    print(f"\nStatistics Test:")
    print(f"Total messages: {stats['total_messages']}")
    print(f"Modules: {stats['modules']}")
    
    return True

if __name__ == "__main__":
    try:
        test_motor_data_parsing()
        test_statistics()
        print("\n✅ All tests passed!")
        sys.exit(0)
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        sys.exit(1)