#!/usr/bin/env python3
"""
Example usage of the VEX WebSocket Client

This script demonstrates how to use the WebSocket client to monitor
VEX robot telemetry data and perform basic analysis.
"""

import time
import json
from websocket_client import VEXWebSocketClient

class MotorAnalyzer:
    """Example analyzer for motor telemetry data."""
    
    def __init__(self):
        self.temperature_alerts = []
        self.performance_data = []
        
    def analyze_motor_data(self, data):
        """Analyze incoming motor data and generate alerts."""
        
        # Temperature monitoring
        if data['motor_temps']:
            for motor, temp in data['motor_temps'].items():
                if temp >= 55:
                    alert = {
                        'timestamp': data['timestamp'],
                        'motor': motor,
                        'temperature': temp,
                        'severity': 'high' if temp >= 60 else 'medium'
                    }
                    self.temperature_alerts.append(alert)
                    print(f"🔥 TEMPERATURE ALERT: {motor.upper()} at {temp}°C")
        
        # Performance monitoring
        if data['motor_details']:
            for motor, details in data['motor_details'].items():
                rpm_efficiency = details['actual_rpm'] / max(details['commanded_rpm'], 1) * 100
                performance = {
                    'timestamp': data['timestamp'],
                    'motor': motor,
                    'efficiency': rpm_efficiency,
                    'current': details['current_amps']
                }
                self.performance_data.append(performance)
                
                if rpm_efficiency < 80:
                    print(f"⚠️  PERFORMANCE: {motor.upper()} efficiency at {rpm_efficiency:.1f}%")
        
        # Battery monitoring
        if data.get('battery_voltage') and data['battery_voltage'] < 11.5:
            print(f"🔋 BATTERY LOW: {data['battery_voltage']}V")
    
    def get_summary(self):
        """Get analysis summary."""
        return {
            'temperature_alerts': len(self.temperature_alerts),
            'recent_alerts': self.temperature_alerts[-5:] if self.temperature_alerts else [],
            'avg_efficiency': sum(p['efficiency'] for p in self.performance_data[-10:]) / min(len(self.performance_data), 10) if self.performance_data else 0
        }

class CustomWebSocketClient(VEXWebSocketClient):
    """Extended WebSocket client with custom analysis."""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.analyzer = MotorAnalyzer()
        self.message_count = 0
        
    def on_message(self, ws, message):
        """Override to add custom analysis."""
        # Call parent method for standard processing
        super().on_message(ws, message)
        
        # Perform custom analysis
        self.message_count += 1
        if self.data_log:
            latest_data = self.data_log[-1]
            self.analyzer.analyze_motor_data(latest_data)
            
            # Print summary every 20 messages
            if self.message_count % 20 == 0:
                summary = self.analyzer.get_summary()
                print(f"\n📊 ANALYSIS SUMMARY (after {self.message_count} messages):")
                print(f"   Temperature alerts: {summary['temperature_alerts']}")
                print(f"   Average efficiency: {summary['avg_efficiency']:.1f}%")
                if summary['recent_alerts']:
                    print(f"   Recent alerts: {len(summary['recent_alerts'])}")
                print()

def main():
    """Example main function."""
    print("VEX WebSocket Client - Advanced Example")
    print("=====================================")
    print("This example demonstrates:")
    print("- Real-time temperature monitoring")
    print("- Motor performance analysis")
    print("- Battery voltage tracking")
    print("- Custom alerts and summaries")
    print("\nPress Ctrl+C to stop\n")
    
    # Create enhanced client
    client = CustomWebSocketClient(
        host="localhost",
        port=7071,
        save_to_file=True  # Save data for later analysis
    )
    
    try:
        client.connect()
    except KeyboardInterrupt:
        print("\n\n🛑 Stopping...")
        
        if client.data_log:
            # Final analysis
            summary = client.analyzer.get_summary()
            stats = client.get_statistics()
            
            print(f"\n📊 FINAL ANALYSIS:")
            print(f"   Total messages received: {len(client.data_log)}")
            print(f"   Temperature alerts: {summary['temperature_alerts']}")
            print(f"   Average motor efficiency: {summary['avg_efficiency']:.1f}%")
            
            if stats.get('battery_readings'):
                avg_battery = sum(stats['battery_readings']) / len(stats['battery_readings'])
                print(f"   Average battery voltage: {avg_battery:.2f}V")
            
            print(f"\n💾 Data saved to CSV file for further analysis")
            print("   Use pandas or Excel to analyze the saved data")
        else:
            print("No data received. Check VEX Extension connection.")

if __name__ == "__main__":
    main()