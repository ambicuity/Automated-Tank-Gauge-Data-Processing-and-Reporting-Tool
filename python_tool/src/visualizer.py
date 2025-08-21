"""
Visualization module for Tank Gauge Data Processing and Reporting Tool

Provides functionality to create visual reports and charts for tank gauge data analysis.
"""

import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional
import os

from tank_gauge_processor import TankGaugeProcessor, TankGaugeData

class TankGaugeVisualizer:
    """Creates visualizations for tank gauge data analysis."""
    
    def __init__(self, processor: TankGaugeProcessor):
        self.processor = processor
        
        # Set up plotting style
        plt.style.use('seaborn-v0_8')
        sns.set_palette("husl")
        
    def plot_fuel_levels_over_time(self, tank_ids: Optional[List[str]] = None, 
                                  save_path: Optional[str] = None) -> None:
        """
        Plot fuel levels over time for specified tanks.
        
        Args:
            tank_ids: List of tank IDs to plot. If None, plots all tanks.
            save_path: Path to save the plot. If None, displays the plot.
        """
        if not self.processor.data_history:
            print("No data available for plotting.")
            return
            
        df = self.processor._to_dataframe()
        
        if tank_ids is None:
            tank_ids = df['tank_id'].unique()
            
        plt.figure(figsize=(12, 8))
        
        for tank_id in tank_ids:
            tank_data = df[df['tank_id'] == tank_id].sort_values('timestamp')
            plt.plot(tank_data['timestamp'], tank_data['fuel_level'], 
                    marker='o', label=f'Tank {tank_id}', linewidth=2)
        
        plt.title('Fuel Levels Over Time', fontsize=16, fontweight='bold')
        plt.xlabel('Timestamp', fontsize=12)
        plt.ylabel('Fuel Level (L)', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.xticks(rotation=45)
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Plot saved to: {save_path}")
        else:
            plt.show()
            
        plt.close()
    
    def plot_consumption_rates(self, save_path: Optional[str] = None) -> None:
        """
        Plot consumption rates for all tanks.
        
        Args:
            save_path: Path to save the plot. If None, displays the plot.
        """
        consumption_rates = {}
        for tank_id in self.processor.tank_capacity.keys():
            rate = self.processor.calculate_consumption_rate(tank_id)
            if rate is not None:
                consumption_rates[tank_id] = rate
                
        if not consumption_rates:
            print("No consumption data available for plotting.")
            return
            
        plt.figure(figsize=(10, 6))
        
        tanks = list(consumption_rates.keys())
        rates = list(consumption_rates.values())
        
        bars = plt.bar(tanks, rates, color='skyblue', edgecolor='navy', alpha=0.7)
        
        # Add value labels on bars
        for bar, rate in zip(bars, rates):
            plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
                    f'{rate:.1f}', ha='center', va='bottom', fontweight='bold')
        
        plt.title('Fuel Consumption Rates by Tank', fontsize=16, fontweight='bold')
        plt.xlabel('Tank ID', fontsize=12)
        plt.ylabel('Consumption Rate (L/hour)', fontsize=12)
        plt.grid(axis='y', alpha=0.3)
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Plot saved to: {save_path}")
        else:
            plt.show()
            
        plt.close()
    
    def plot_tank_utilization(self, save_path: Optional[str] = None) -> None:
        """
        Plot current tank utilization as a pie chart and bar chart.
        
        Args:
            save_path: Path to save the plot. If None, displays the plot.
        """
        current_levels = self.processor.get_current_fuel_levels()
        
        if not current_levels:
            print("No current level data available for plotting.")
            return
            
        # Calculate utilization percentages
        utilization = {}
        for tank_id, level in current_levels.items():
            if tank_id in self.processor.tank_capacity:
                utilization[tank_id] = (level / self.processor.tank_capacity[tank_id]) * 100
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))
        
        # Bar chart
        tanks = list(utilization.keys())
        percentages = list(utilization.values())
        
        colors = ['red' if p < 20 else 'orange' if p < 50 else 'green' for p in percentages]
        
        bars = ax1.bar(tanks, percentages, color=colors, alpha=0.7, edgecolor='black')
        
        # Add percentage labels
        for bar, pct in zip(bars, percentages):
            ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 1,
                    f'{pct:.1f}%', ha='center', va='bottom', fontweight='bold')
        
        ax1.set_title('Tank Utilization Levels', fontsize=14, fontweight='bold')
        ax1.set_xlabel('Tank ID')
        ax1.set_ylabel('Utilization (%)')
        ax1.set_ylim(0, 105)
        ax1.grid(axis='y', alpha=0.3)
        
        # Add horizontal lines for warning levels
        ax1.axhline(y=20, color='red', linestyle='--', alpha=0.7, label='Critical (20%)')
        ax1.axhline(y=50, color='orange', linestyle='--', alpha=0.7, label='Warning (50%)')
        ax1.legend()
        
        # Pie chart for total fuel distribution
        fuel_amounts = [current_levels[tank_id] for tank_id in tanks]
        
        ax2.pie(fuel_amounts, labels=tanks, autopct='%1.1f%%', startangle=90)
        ax2.set_title('Fuel Distribution Across Tanks', fontsize=14, fontweight='bold')
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Plot saved to: {save_path}")
        else:
            plt.show()
            
        plt.close()
    
    def plot_temperature_pressure_correlation(self, save_path: Optional[str] = None) -> None:
        """
        Plot temperature vs pressure correlation for all tanks.
        
        Args:
            save_path: Path to save the plot. If None, displays the plot.
        """
        if not self.processor.data_history:
            print("No data available for plotting.")
            return
            
        df = self.processor._to_dataframe()
        
        plt.figure(figsize=(10, 8))
        
        # Scatter plot colored by tank
        for tank_id in df['tank_id'].unique():
            tank_data = df[df['tank_id'] == tank_id]
            plt.scatter(tank_data['temperature'], tank_data['pressure'], 
                       label=f'Tank {tank_id}', alpha=0.6, s=50)
        
        # Add trend line
        z = np.polyfit(df['temperature'], df['pressure'], 1)
        p = np.poly1d(z)
        plt.plot(df['temperature'], p(df['temperature']), "r--", alpha=0.8, 
                linewidth=2, label=f'Trend line (slope: {z[0]:.2f})')
        
        plt.title('Temperature vs Pressure Correlation', fontsize=16, fontweight='bold')
        plt.xlabel('Temperature (°C)', fontsize=12)
        plt.ylabel('Pressure (kPa)', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Plot saved to: {save_path}")
        else:
            plt.show()
            
        plt.close()
    
    def create_comprehensive_dashboard(self, output_dir: str = "reports") -> None:
        """
        Create a comprehensive dashboard with all visualizations.
        
        Args:
            output_dir: Directory to save all plots
        """
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)
            
        print("Generating comprehensive dashboard...")
        
        # Generate all plots
        plots = [
            ("fuel_levels_over_time.png", self.plot_fuel_levels_over_time),
            ("consumption_rates.png", self.plot_consumption_rates),
            ("tank_utilization.png", self.plot_tank_utilization),
            ("temperature_pressure_correlation.png", self.plot_temperature_pressure_correlation)
        ]
        
        for filename, plot_func in plots:
            filepath = os.path.join(output_dir, filename)
            try:
                plot_func(save_path=filepath)
            except Exception as e:
                print(f"Error creating {filename}: {str(e)}")
        
        print(f"Dashboard created in: {output_dir}")
        
    def generate_leak_alert_visualization(self, leaks: List[Dict], 
                                        save_path: Optional[str] = None) -> None:
        """
        Create visualization for leak alerts.
        
        Args:
            leaks: List of leak alerts from detect_potential_leaks()
            save_path: Path to save the plot
        """
        if not leaks:
            print("No leaks detected - no visualization needed.")
            return
            
        # Extract data for plotting
        tank_ids = [leak['tank_id'] for leak in leaks]
        severities = [leak['severity'] for leak in leaks]
        rates = [leak['consumption_rate'] for leak in leaks]
        thresholds = [leak['threshold'] for leak in leaks]
        
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))
        
        # Bar chart of consumption rates vs thresholds
        x_pos = np.arange(len(tank_ids))
        
        color_map = {'HIGH': 'red', 'MEDIUM': 'orange', 'LOW': 'yellow'}
        colors = [color_map.get(severity, 'blue') for severity in severities]
        
        bars1 = ax1.bar(x_pos - 0.2, rates, 0.4, label='Actual Rate', 
                       color=colors, alpha=0.7)
        bars2 = ax1.bar(x_pos + 0.2, thresholds, 0.4, label='Threshold', 
                       color='green', alpha=0.7)
        
        ax1.set_xlabel('Tank ID')
        ax1.set_ylabel('Consumption Rate (L/hour)')
        ax1.set_title('Leak Detection: Consumption Rates vs Thresholds')
        ax1.set_xticks(x_pos)
        ax1.set_xticklabels(tank_ids)
        ax1.legend()
        ax1.grid(axis='y', alpha=0.3)
        
        # Add value labels
        for bar, value in zip(bars1, rates):
            ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
                    f'{value:.1f}', ha='center', va='bottom', fontsize=10)
        
        # Severity distribution pie chart
        severity_counts = {}
        for severity in severities:
            severity_counts[severity] = severity_counts.get(severity, 0) + 1
        
        ax2.pie(severity_counts.values(), labels=severity_counts.keys(), 
               autopct='%1.0f%%', colors=[color_map[s] for s in severity_counts.keys()])
        ax2.set_title('Leak Alert Severity Distribution')
        
        plt.tight_layout()
        
        if save_path:
            plt.savefig(save_path, dpi=300, bbox_inches='tight')
            print(f"Leak alert visualization saved to: {save_path}")
        else:
            plt.show()
            
        plt.close()