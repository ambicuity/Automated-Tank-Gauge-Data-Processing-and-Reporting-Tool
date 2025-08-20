import sys
import os
import pytest
import pandas as pd
from datetime import datetime, timedelta
import tempfile
import json

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from tank_gauge_processor import TankGaugeProcessor, TankGaugeData, TankGaugeSimulator

class TestTankGaugeData:
    """Test the TankGaugeData class."""
    
    def test_creation(self):
        """Test creating a TankGaugeData instance."""
        timestamp = datetime.now()
        data = TankGaugeData("TANK_001", timestamp, 1000.0, 25.5, 101.3)
        
        assert data.tank_id == "TANK_001"
        assert data.timestamp == timestamp
        assert data.fuel_level == 1000.0
        assert data.temperature == 25.5
        assert data.pressure == 101.3

class TestTankGaugeProcessor:
    """Test the TankGaugeProcessor class."""
    
    @pytest.fixture
    def tank_capacities(self):
        """Sample tank capacities for testing."""
        return {
            'TANK_001': 10000.0,
            'TANK_002': 15000.0,
            'TANK_003': 8000.0
        }
    
    @pytest.fixture
    def processor(self, tank_capacities):
        """Create a processor instance for testing."""
        return TankGaugeProcessor(tank_capacities)
    
    @pytest.fixture
    def sample_data(self):
        """Create sample tank gauge data."""
        base_time = datetime.now() - timedelta(hours=24)
        data = []
        
        for i in range(48):  # 48 hours of hourly data
            timestamp = base_time + timedelta(hours=i)
            
            # Simulate decreasing fuel levels
            data.append(TankGaugeData("TANK_001", timestamp, 5000 - i*10, 20 + i*0.1, 101.3))
            data.append(TankGaugeData("TANK_002", timestamp, 8000 - i*15, 22 + i*0.05, 101.5))
            data.append(TankGaugeData("TANK_003", timestamp, 4000 - i*8, 18 + i*0.2, 101.1))
        
        return data
    
    def test_initialization(self, tank_capacities):
        """Test processor initialization."""
        processor = TankGaugeProcessor(tank_capacities)
        assert processor.tank_capacity == tank_capacities
        assert processor.data_history == []
        assert processor.leak_threshold == 0.05
    
    def test_add_data_point(self, processor):
        """Test adding data points."""
        data = TankGaugeData("TANK_001", datetime.now(), 1000.0, 25.0, 101.3)
        processor.add_data_point(data)
        
        assert len(processor.data_history) == 1
        assert processor.data_history[0] == data
    
    def test_get_current_fuel_levels(self, processor, sample_data):
        """Test getting current fuel levels."""
        for data in sample_data:
            processor.add_data_point(data)
        
        current_levels = processor.get_current_fuel_levels()
        
        assert len(current_levels) == 3
        assert "TANK_001" in current_levels
        assert "TANK_002" in current_levels
        assert "TANK_003" in current_levels
        
        # Should be the most recent values
        assert current_levels["TANK_001"] == 5000 - 47*10  # Last data point
    
    def test_calculate_consumption_rate(self, processor, sample_data):
        """Test consumption rate calculation."""
        for data in sample_data:
            processor.add_data_point(data)
        
        rate = processor.calculate_consumption_rate("TANK_001", hours=24)
        
        assert rate is not None
        assert rate > 0  # Should be positive (consumption)
        assert abs(rate - 10.0) < 1.0  # Should be approximately 10 L/hour
    
    def test_detect_potential_leaks_normal(self, processor, sample_data):
        """Test leak detection with normal consumption."""
        for data in sample_data:
            processor.add_data_point(data)
        
        leaks = processor.detect_potential_leaks()
        
        # With normal consumption rates, should not detect leaks
        assert len(leaks) == 0
    
    def test_detect_potential_leaks_with_leak(self, processor, tank_capacities):
        """Test leak detection with abnormal consumption."""
        # Set a lower leak threshold for testing
        processor.leak_threshold = 0.01  # 1% instead of 5%
        
        base_time = datetime.now() - timedelta(hours=6)
        
        # Create data with high consumption rate (leak scenario)
        for i in range(24):  # 6 hours of 15-minute intervals
            timestamp = base_time + timedelta(minutes=i*15)
            # High consumption rate: 100 L/hour instead of normal ~10 L/hour
            fuel_level = 5000 - i*25  # 100 L/hour consumption
            
            data = TankGaugeData("TANK_001", timestamp, fuel_level, 20.0, 101.3)
            processor.add_data_point(data)
        
        # Also add a recent data point to ensure we can calculate the 6-hour rate
        recent_time = datetime.now() - timedelta(minutes=1)  # Very recent
        recent_data = TankGaugeData("TANK_001", recent_time, 4400, 20.0, 101.3)  # Continuing the pattern
        processor.add_data_point(recent_data)
        
        leaks = processor.detect_potential_leaks()
        
        # Should detect leak for TANK_001
        assert len(leaks) > 0
        leak = leaks[0]
        assert leak['tank_id'] == 'TANK_001'
        assert leak['severity'] in ['MEDIUM', 'HIGH']
        assert leak['consumption_rate'] > 50.0  # Should be high
    
    def test_generate_summary_report(self, processor, sample_data):
        """Test report generation."""
        for data in sample_data:
            processor.add_data_point(data)
        
        report = processor.generate_summary_report()
        
        assert 'report_timestamp' in report
        assert 'total_tanks' in report
        assert 'current_fuel_levels' in report
        assert 'tank_utilization_percent' in report
        assert 'consumption_rates_l_per_hour' in report
        assert 'potential_leaks' in report
        assert 'data_points_analyzed' in report
        assert 'data_time_range' in report
        
        assert report['total_tanks'] == 3
        assert report['data_points_analyzed'] == len(sample_data)
        assert len(report['current_fuel_levels']) == 3
    
    def test_load_and_save_data(self, processor, sample_data, tank_capacities):
        """Test loading data from CSV and saving report."""
        # Create temporary CSV file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.csv', delete=False) as f:
            csv_file = f.name
            
            # Write CSV header
            f.write("tank_id,timestamp,fuel_level,temperature,pressure\n")
            
            # Write sample data
            for data in sample_data[:10]:  # Use first 10 data points
                f.write(f"{data.tank_id},{data.timestamp.isoformat()},{data.fuel_level},{data.temperature},{data.pressure}\n")
        
        try:
            # Load data from CSV
            processor.load_data_from_csv(csv_file)
            assert len(processor.data_history) == 10
            
            # Generate and save report
            with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
                report_file = f.name
            
            processor.save_report(report_file)
            
            # Verify report was saved
            assert os.path.exists(report_file)
            
            with open(report_file, 'r') as f:
                saved_report = json.load(f)
            
            assert 'report_timestamp' in saved_report
            assert saved_report['data_points_analyzed'] == 10
            
            # Cleanup
            os.unlink(report_file)
            
        finally:
            os.unlink(csv_file)
    
    def test_empty_data_handling(self, processor):
        """Test handling of empty data."""
        # Test with no data
        current_levels = processor.get_current_fuel_levels()
        assert current_levels == {}
        
        consumption_rate = processor.calculate_consumption_rate("TANK_001")
        assert consumption_rate is None
        
        leaks = processor.detect_potential_leaks()
        assert leaks == []
        
        report = processor.generate_summary_report()
        assert 'error' in report

class TestTankGaugeSimulator:
    """Test the TankGaugeSimulator class."""
    
    @pytest.fixture
    def tank_capacities(self):
        return {
            'TANK_001': 10000.0,
            'TANK_002': 15000.0
        }
    
    @pytest.fixture
    def simulator(self, tank_capacities):
        return TankGaugeSimulator(tank_capacities)
    
    def test_initialization(self, tank_capacities):
        """Test simulator initialization."""
        simulator = TankGaugeSimulator(tank_capacities)
        assert simulator.tank_capacity == tank_capacities
        
        # Should start at 80% capacity
        for tank_id, capacity in tank_capacities.items():
            assert simulator.current_levels[tank_id] == capacity * 0.8
    
    def test_simulate_normal_consumption(self, simulator):
        """Test normal consumption simulation."""
        data_points = simulator.simulate_normal_consumption("TANK_001", hours=2)
        
        # Should generate data points every 15 minutes for 2 hours = 8 points
        assert len(data_points) == 8
        
        # Check that fuel level decreases over time
        levels = [data.fuel_level for data in data_points]
        for i in range(1, len(levels)):
            assert levels[i] <= levels[i-1]  # Should decrease or stay same
        
        # Check data structure
        for data in data_points:
            assert data.tank_id == "TANK_001"
            assert isinstance(data.fuel_level, float)
            assert isinstance(data.temperature, float)
            assert isinstance(data.pressure, float)
            assert data.fuel_level >= 0  # Should not go negative
    
    def test_simulate_leak_scenario(self, simulator):
        """Test leak scenario simulation."""
        initial_level = simulator.current_levels["TANK_001"]
        data_points = simulator.simulate_leak_scenario("TANK_001", leak_rate=20.0, duration_hours=1)
        
        # Should generate 4 points for 1 hour (every 15 minutes)
        assert len(data_points) == 4
        
        # Should show faster consumption than normal
        final_level = data_points[-1].fuel_level
        consumed = initial_level - final_level
        
        # Should consume more than normal rate (normal ~5 L/hour, leak adds 20 L/hour)
        assert consumed > 20.0  # Should be roughly 25 L for 1 hour
    
    def test_data_point_structure(self, simulator):
        """Test that generated data points have correct structure."""
        data_points = simulator.simulate_normal_consumption("TANK_001", hours=0.5)
        
        for data in data_points:
            assert hasattr(data, 'tank_id')
            assert hasattr(data, 'timestamp')
            assert hasattr(data, 'fuel_level')
            assert hasattr(data, 'temperature')
            assert hasattr(data, 'pressure')
            
            # Check reasonable ranges
            assert data.fuel_level >= 0
            assert -10 <= data.temperature <= 50  # Reasonable temperature range
            assert 90 <= data.pressure <= 120  # Reasonable pressure range

if __name__ == "__main__":
    # Run tests with pytest
    pytest.main([__file__, "-v"])