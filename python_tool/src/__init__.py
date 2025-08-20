"""
Automated Tank Gauge Data Processing and Reporting Tool

This package provides comprehensive functionality for processing and analyzing
data from automated tank gauge systems, including:

- Data processing and validation
- Consumption rate analysis
- Leak detection algorithms
- Report generation
- Data visualization

Main classes:
- TankGaugeProcessor: Core data processing and analysis
- TankGaugeSimulator: Simulates tank data for testing and demonstration
- TankGaugeVisualizer: Creates charts and visual reports
"""

from .tank_gauge_processor import (
    TankGaugeData,
    TankGaugeProcessor,
    TankGaugeSimulator
)

try:
    from .visualizer import TankGaugeVisualizer
except ImportError:
    # Visualizer may not be available if matplotlib is not installed
    TankGaugeVisualizer = None

__version__ = "1.0.0"
__author__ = "Automated Tank Gauge Team"

__all__ = [
    'TankGaugeData',
    'TankGaugeProcessor', 
    'TankGaugeSimulator',
    'TankGaugeVisualizer'
]