#!/usr/bin/env python3
"""
Main application for Automated Tank Gauge Data Processing and Reporting Tool

This script demonstrates the functionality of the tank gauge data processing system.
"""

import argparse
import sys
import os
from datetime import datetime, timedelta
import json

# Add src to path for imports
sys.path.append(os.path.join(os.path.dirname(__file__), 'src'))

from tank_gauge_processor import TankGaugeProcessor, TankGaugeSimulator
from visualizer import TankGaugeVisualizer

def create_sample_data(output_file: str = "data/sample_tank_data.csv"):
    """Create sample tank gauge data for demonstration."""
    
    # Define tank capacities
    tank_capacities = {
        'TANK_001': 10000.0,  # 10,000 L capacity
        'TANK_002': 15000.0,  # 15,000 L capacity  
        'TANK_003': 8000.0,   # 8,000 L capacity
    }
    
    # Create simulator
    simulator = TankGaugeSimulator(tank_capacities)
    
    # Generate sample data
    all_data = []
    
    # Normal operation for all tanks (last 48 hours)
    for tank_id in tank_capacities.keys():
        normal_data = simulator.simulate_normal_consumption(tank_id, 48)
        all_data.extend(normal_data)
    
    # Add a leak scenario for TANK_002 (last 6 hours)
    leak_data = simulator.simulate_leak_scenario('TANK_002', leak_rate=20.0, duration_hours=6)
    all_data.extend(leak_data)
    
    # Sort by timestamp
    all_data.sort(key=lambda x: x.timestamp)
    
    # Create DataFrame and save to CSV
    import pandas as pd
    
    data_dict = []
    for data in all_data:
        data_dict.append({
            'tank_id': data.tank_id,
            'timestamp': data.timestamp.isoformat(),
            'fuel_level': data.fuel_level,
            'temperature': data.temperature,
            'pressure': data.pressure
        })
    
    df = pd.DataFrame(data_dict)
    
    # Ensure data directory exists
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    df.to_csv(output_file, index=False)
    print(f"Sample data created: {output_file}")
    print(f"Generated {len(all_data)} data points for {len(tank_capacities)} tanks")
    
    return tank_capacities

def demonstrate_processing(data_file: str, tank_capacities: dict):
    """Demonstrate the tank gauge processing capabilities."""
    
    print("\n" + "="*60)
    print("AUTOMATED TANK GAUGE DATA PROCESSING DEMONSTRATION")
    print("="*60)
    
    # Initialize processor
    processor = TankGaugeProcessor(tank_capacities)
    
    # Load data
    print(f"\nLoading data from: {data_file}")
    processor.load_data_from_csv(data_file)
    print(f"Loaded {len(processor.data_history)} data points")
    
    # Display current fuel levels
    print("\n📊 CURRENT FUEL LEVELS")
    print("-" * 30)
    current_levels = processor.get_current_fuel_levels()
    for tank_id, level in current_levels.items():
        capacity = tank_capacities[tank_id]
        utilization = (level / capacity) * 100
        status = "🔴 CRITICAL" if utilization < 20 else "🟡 WARNING" if utilization < 50 else "🟢 NORMAL"
        print(f"{tank_id}: {level:,.1f} L ({utilization:.1f}% capacity) {status}")
    
    # Display consumption rates
    print("\n⛽ CONSUMPTION ANALYSIS")
    print("-" * 30)
    for tank_id in tank_capacities.keys():
        rate_24h = processor.calculate_consumption_rate(tank_id, 24)
        rate_6h = processor.calculate_consumption_rate(tank_id, 6)
        
        if rate_24h is not None and rate_6h is not None:
            print(f"{tank_id}:")
            print(f"  24-hour rate: {rate_24h:.2f} L/hour")
            print(f"   6-hour rate: {rate_6h:.2f} L/hour")
            
            # Check for acceleration in consumption
            if rate_6h > rate_24h * 1.5:
                print(f"  ⚠️  ALERT: Recent consumption rate increased by {((rate_6h/rate_24h-1)*100):.1f}%")
        else:
            print(f"{tank_id}: Insufficient data for consumption analysis")
    
    # Leak detection
    print("\n🚨 LEAK DETECTION ANALYSIS")
    print("-" * 30)
    leaks = processor.detect_potential_leaks()
    
    if leaks:
        print(f"⚠️  {len(leaks)} potential leak(s) detected:")
        for leak in leaks:
            print(f"\n🔍 {leak['tank_id']} - {leak['severity']} SEVERITY")
            print(f"   Consumption Rate: {leak['consumption_rate']:.2f} L/hour")
            print(f"   Threshold: {leak['threshold']:.2f} L/hour")
            print(f"   Details: {leak['details']}")
    else:
        print("✅ No potential leaks detected")
    
    # Generate comprehensive report
    print("\n📋 GENERATING COMPREHENSIVE REPORT")
    print("-" * 30)
    report = processor.generate_summary_report()
    
    # Save report
    report_file = "reports/tank_gauge_report.json"
    os.makedirs(os.path.dirname(report_file), exist_ok=True)
    processor.save_report(report_file, report)
    print(f"Report saved to: {report_file}")
    
    # Create visualizations
    print("\n📈 CREATING VISUALIZATIONS")
    print("-" * 30)
    visualizer = TankGaugeVisualizer(processor)
    
    try:
        visualizer.create_comprehensive_dashboard("reports")
        
        # Create leak visualization if leaks detected
        if leaks:
            visualizer.generate_leak_alert_visualization(leaks, "reports/leak_alerts.png")
            
    except Exception as e:
        print(f"Visualization error (this may be due to display limitations): {str(e)}")
        print("Reports are still available in JSON format.")
    
    print("\n✅ PROCESSING COMPLETE")
    print(f"📁 All reports saved to: reports/")
    
    return processor, visualizer

def main():
    """Main application entry point."""
    
    parser = argparse.ArgumentParser(
        description='Automated Tank Gauge Data Processing and Reporting Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python main.py --demo                    # Run full demonstration
  python main.py --create-data            # Create sample data only
  python main.py --process data.csv       # Process specific data file
        """
    )
    
    parser.add_argument('--demo', action='store_true',
                       help='Run full demonstration with sample data')
    parser.add_argument('--create-data', action='store_true',
                       help='Create sample data only')
    parser.add_argument('--process', type=str, metavar='DATA_FILE',
                       help='Process specific CSV data file')
    parser.add_argument('--output-dir', type=str, default='reports',
                       help='Output directory for reports (default: reports)')
    
    args = parser.parse_args()
    
    if args.demo:
        # Full demonstration
        print("🚀 Starting Automated Tank Gauge Data Processing Demonstration")
        tank_capacities = create_sample_data()
        demonstrate_processing("data/sample_tank_data.csv", tank_capacities)
        
    elif args.create_data:
        # Create sample data only
        print("📊 Creating sample tank gauge data...")
        create_sample_data()
        
    elif args.process:
        # Process specific file
        if not os.path.exists(args.process):
            print(f"❌ Error: Data file '{args.process}' not found")
            sys.exit(1)
            
        # You would need to specify tank capacities for real data
        tank_capacities = {
            'TANK_001': 10000.0,
            'TANK_002': 15000.0,
            'TANK_003': 8000.0,
        }
        
        demonstrate_processing(args.process, tank_capacities)
        
    else:
        parser.print_help()

if __name__ == "__main__":
    main()