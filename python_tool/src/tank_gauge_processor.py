"""
Automated Tank Gauge Data Processing and Reporting Tool

This module provides functionality to process data from simulated Automated Tank Gauges
and generate reports on fuel levels, consumption, and potential leaks.
"""

import pandas as pd
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
import json
import warnings

class TankGaugeData:
    """Represents data from a tank gauge sensor."""
    
    def __init__(self, tank_id: str, timestamp: datetime, fuel_level: float, 
                 temperature: float, pressure: float):
        self.tank_id = tank_id
        self.timestamp = timestamp
        self.fuel_level = fuel_level  # in liters
        self.temperature = temperature  # in Celsius
        self.pressure = pressure  # in kPa

class TankGaugeProcessor:
    """Main class for processing tank gauge data and generating reports."""
    
    def __init__(self, tank_capacity: Dict[str, float]):
        """
        Initialize the processor with tank capacities.
        
        Args:
            tank_capacity: Dictionary mapping tank_id to capacity in liters
        """
        self.tank_capacity = tank_capacity
        self.data_history: List[TankGaugeData] = []
        self.leak_threshold = 0.05  # 5% capacity loss per hour indicates potential leak
        
    def add_data_point(self, data: TankGaugeData) -> None:
        """Add a new data point to the processing history."""
        self.data_history.append(data)
        
    def load_data_from_csv(self, filepath: str) -> None:
        """Load data from CSV file."""
        try:
            df = pd.read_csv(filepath)
            df['timestamp'] = pd.to_datetime(df['timestamp'])
            
            for _, row in df.iterrows():
                data = TankGaugeData(
                    tank_id=row['tank_id'],
                    timestamp=row['timestamp'],
                    fuel_level=row['fuel_level'],
                    temperature=row['temperature'],
                    pressure=row['pressure']
                )
                self.add_data_point(data)
                
        except Exception as e:
            raise RuntimeError(f"Failed to load data from {filepath}: {str(e)}")
    
    def get_current_fuel_levels(self) -> Dict[str, float]:
        """Get the most recent fuel level for each tank."""
        if not self.data_history:
            return {}
            
        df = self._to_dataframe()
        latest_data = df.loc[df.groupby('tank_id')['timestamp'].idxmax()]
        
        return dict(zip(latest_data['tank_id'], latest_data['fuel_level']))
    
    def calculate_consumption_rate(self, tank_id: str, hours: int = 24) -> Optional[float]:
        """
        Calculate fuel consumption rate for a specific tank over the last N hours.
        
        Args:
            tank_id: Tank identifier
            hours: Number of hours to look back
            
        Returns:
            Consumption rate in liters per hour, or None if insufficient data
        """
        if not self.data_history:
            return None
            
        cutoff_time = datetime.now() - timedelta(hours=hours)
        tank_data = [d for d in self.data_history 
                    if d.tank_id == tank_id and d.timestamp >= cutoff_time]
        
        if len(tank_data) < 2:
            return None
            
        # Sort by timestamp
        tank_data.sort(key=lambda x: x.timestamp)
        
        # Calculate rate based on first and last readings
        time_diff = (tank_data[-1].timestamp - tank_data[0].timestamp).total_seconds() / 3600  # hours
        fuel_diff = tank_data[0].fuel_level - tank_data[-1].fuel_level  # consumption is positive
        
        if time_diff == 0:
            return None
            
        return fuel_diff / time_diff
    
    def detect_potential_leaks(self) -> List[Dict]:
        """
        Detect potential leaks based on abnormal fuel level drops.
        
        Returns:
            List of potential leak alerts with tank_id, severity, and details
        """
        leaks = []
        
        for tank_id in self.tank_capacity.keys():
            # Check consumption rate over last 6 hours
            recent_rate = self.calculate_consumption_rate(tank_id, hours=6)
            
            if recent_rate is None:
                continue
                
            # Check if consumption rate is abnormally high
            capacity = self.tank_capacity[tank_id]
            leak_threshold_rate = capacity * self.leak_threshold
            
            if recent_rate > leak_threshold_rate:
                severity = "HIGH" if recent_rate > leak_threshold_rate * 2 else "MEDIUM"
                
                leaks.append({
                    'tank_id': tank_id,
                    'severity': severity,
                    'consumption_rate': recent_rate,
                    'threshold': leak_threshold_rate,
                    'timestamp': datetime.now(),
                    'details': f"Consumption rate {recent_rate:.2f} L/h exceeds threshold {leak_threshold_rate:.2f} L/h"
                })
                
        return leaks
    
    def _to_dataframe(self) -> pd.DataFrame:
        """Convert data history to pandas DataFrame for analysis."""
        data = []
        for d in self.data_history:
            data.append({
                'tank_id': d.tank_id,
                'timestamp': d.timestamp,
                'fuel_level': d.fuel_level,
                'temperature': d.temperature,
                'pressure': d.pressure
            })
        return pd.DataFrame(data)
    
    def generate_summary_report(self) -> Dict:
        """Generate a comprehensive summary report."""
        if not self.data_history:
            return {'error': 'No data available for report generation'}
            
        df = self._to_dataframe()
        current_levels = self.get_current_fuel_levels()
        potential_leaks = self.detect_potential_leaks()
        
        # Calculate utilization percentages
        utilization = {}
        for tank_id, level in current_levels.items():
            if tank_id in self.tank_capacity:
                utilization[tank_id] = (level / self.tank_capacity[tank_id]) * 100
        
        # Calculate consumption rates for all tanks
        consumption_rates = {}
        for tank_id in self.tank_capacity.keys():
            rate = self.calculate_consumption_rate(tank_id)
            if rate is not None:
                consumption_rates[tank_id] = rate
        
        report = {
            'report_timestamp': datetime.now().isoformat(),
            'total_tanks': len(self.tank_capacity),
            'current_fuel_levels': current_levels,
            'tank_utilization_percent': utilization,
            'consumption_rates_l_per_hour': consumption_rates,
            'potential_leaks': potential_leaks,
            'data_points_analyzed': len(self.data_history),
            'data_time_range': {
                'start': min(d.timestamp for d in self.data_history).isoformat(),
                'end': max(d.timestamp for d in self.data_history).isoformat()
            }
        }
        
        return report
    
    def save_report(self, filepath: str, report: Dict = None) -> None:
        """Save report to JSON file."""
        if report is None:
            report = self.generate_summary_report()
            
        try:
            with open(filepath, 'w') as f:
                json.dump(report, f, indent=2, default=str)
        except Exception as e:
            raise RuntimeError(f"Failed to save report to {filepath}: {str(e)}")

class TankGaugeSimulator:
    """Simulates tank gauge data for testing and demonstration purposes."""
    
    def __init__(self, tank_capacity: Dict[str, float]):
        self.tank_capacity = tank_capacity
        self.current_levels = {tank_id: capacity * 0.8 for tank_id, capacity in tank_capacity.items()}
        
    def simulate_normal_consumption(self, tank_id: str, hours: float) -> List[TankGaugeData]:
        """Simulate normal fuel consumption over time."""
        data_points = []
        base_consumption_rate = 5.0  # L/hour base rate
        current_level = self.current_levels[tank_id]
        
        # Generate data points every 15 minutes
        intervals = int(hours * 4)
        
        for i in range(intervals):
            timestamp = datetime.now() - timedelta(hours=hours) + timedelta(minutes=i*15)
            
            # Add some randomness to consumption
            consumption = base_consumption_rate * 0.25 + np.random.normal(0, 0.5)
            consumption = max(0, consumption)  # Ensure non-negative
            
            current_level -= consumption
            current_level = max(0, current_level)  # Ensure non-negative
            
            # Simulate temperature and pressure variations
            temperature = 20 + np.random.normal(0, 5)  # °C
            pressure = 101.3 + np.random.normal(0, 2)  # kPa
            
            data_point = TankGaugeData(
                tank_id=tank_id,
                timestamp=timestamp,
                fuel_level=current_level,
                temperature=temperature,
                pressure=pressure
            )
            data_points.append(data_point)
        
        self.current_levels[tank_id] = current_level
        return data_points
    
    def simulate_leak_scenario(self, tank_id: str, leak_rate: float, duration_hours: float) -> List[TankGaugeData]:
        """Simulate a leak scenario with accelerated fuel loss."""
        data_points = []
        normal_consumption = 5.0  # L/hour
        total_rate = normal_consumption + leak_rate
        current_level = self.current_levels[tank_id]
        
        intervals = int(duration_hours * 4)  # Every 15 minutes
        
        for i in range(intervals):
            timestamp = datetime.now() - timedelta(hours=duration_hours) + timedelta(minutes=i*15)
            
            # Consumption includes leak
            consumption = total_rate * 0.25  # 15 minutes worth
            current_level -= consumption
            current_level = max(0, current_level)
            
            temperature = 20 + np.random.normal(0, 3)
            pressure = 101.3 + np.random.normal(0, 1.5)
            
            data_point = TankGaugeData(
                tank_id=tank_id,
                timestamp=timestamp,
                fuel_level=current_level,
                temperature=temperature,
                pressure=pressure
            )
            data_points.append(data_point)
            
        self.current_levels[tank_id] = current_level
        return data_points