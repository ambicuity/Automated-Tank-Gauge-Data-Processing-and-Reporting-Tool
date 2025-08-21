#include "tank_gauge_protocol.h"
#include "server_communication.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <random>
#include <signal.h>
#include <atomic>
#include <iomanip>

using namespace TankGauge;

// Global flag for clean shutdown
std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ". Shutting down gracefully..." << std::endl;
    running.store(false);
}

class TankGaugeSimulator {
private:
    std::mt19937 gen_;
    std::normal_distribution<float> temp_dist_;
    std::normal_distribution<float> pressure_dist_;
    std::normal_distribution<float> flow_dist_;
    
    float current_fuel_level_;
    float tank_capacity_;
    float base_consumption_rate_;
    
public:
    TankGaugeSimulator(float tank_capacity) 
        : gen_(std::random_device{}()),
          temp_dist_(20.0f, 5.0f),      // 20°C ± 5°C
          pressure_dist_(101.3f, 2.0f), // 101.3 kPa ± 2 kPa
          flow_dist_(0.0f, 0.5f),       // Flow rate variation
          current_fuel_level_(tank_capacity * 0.8f), // Start at 80%
          tank_capacity_(tank_capacity),
          base_consumption_rate_(5.0f) { // 5 L/hour base consumption
    }
    
    TankData generateData() {
        TankData data;
        
        // Simulate fuel consumption
        float consumption = base_consumption_rate_ / 3600.0f; // Per second
        current_fuel_level_ -= consumption;
        current_fuel_level_ = std::max(0.0f, current_fuel_level_);
        
        data.fuel_level = current_fuel_level_;
        data.temperature = temp_dist_(gen_);
        data.pressure = pressure_dist_(gen_);
        data.flow_rate = std::abs(flow_dist_(gen_));
        
        return data;
    }
    
    float getFuelLevel() const { return current_fuel_level_; }
    float getTankCapacity() const { return tank_capacity_; }
    
    // Simulate a leak by increasing consumption rate
    void simulateLeak(float leak_rate) {
        base_consumption_rate_ += leak_rate;
        std::cout << "LEAK SIMULATION: Consumption rate increased to " 
                  << base_consumption_rate_ << " L/hour" << std::endl;
    }
};

void demonstrateProtocol() {
    std::cout << "\n=== Tank Gauge Communication Protocol Demonstration ===" << std::endl;
    
    // Create protocol instance
    TankGaugeProtocol protocol("TANK_DEMO_001");
    
    // Create some sample data
    TankGaugeSimulator simulator(10000.0f); // 10,000 L tank
    
    std::cout << "\nCreating and serializing different message types:" << std::endl;
    
    // 1. Heartbeat message
    auto heartbeat_msg = protocol.createHeartbeatMessage();
    auto heartbeat_data = heartbeat_msg->serialize();
    std::cout << "✓ Heartbeat message: " << heartbeat_data.size() << " bytes" << std::endl;
    
    // 2. Data transmission message
    TankData tank_data = simulator.generateData();
    auto data_msg = protocol.createDataMessage(tank_data);
    auto data_msg_serialized = data_msg->serialize();
    std::cout << "✓ Data message: " << data_msg_serialized.size() << " bytes" << std::endl;
    std::cout << "  Tank data: " << tank_data.fuel_level << "L, " 
              << tank_data.temperature << "°C, " << tank_data.pressure << " kPa" << std::endl;
    
    // 3. Alert message
    AlertData alert;
    alert.severity = AlertSeverity::HIGH;
    alert.description = "Low fuel level detected";
    alert.threshold_value = 1000.0f;
    alert.current_value = tank_data.fuel_level;
    
    auto alert_msg = protocol.createAlertMessage(alert);
    auto alert_data = alert_msg->serialize();
    std::cout << "✓ Alert message: " << alert_data.size() << " bytes" << std::endl;
    std::cout << "  Alert: " << alert.description << " (Current: " 
              << alert.current_value << ", Threshold: " << alert.threshold_value << ")" << std::endl;
    
    // 4. Test message deserialization
    std::cout << "\nTesting message deserialization:" << std::endl;
    
    auto deserialized_data = Message::deserialize(data_msg_serialized);
    if (deserialized_data && deserialized_data->validate()) {
        std::cout << "✓ Data message deserialization successful" << std::endl;
        std::cout << "  Message ID: " << deserialized_data->message_id << std::endl;
        std::cout << "  Tank ID: " << deserialized_data->tank_id << std::endl;
        std::cout << "  Payload size: " << deserialized_data->payload.size() << " bytes" << std::endl;
        
        // Deserialize tank data
        TankData recovered_data = TankData::deserialize(deserialized_data->payload);
        std::cout << "  Recovered data: " << recovered_data.fuel_level << "L, " 
                  << recovered_data.temperature << "°C" << std::endl;
    } else {
        std::cout << "✗ Data message deserialization failed" << std::endl;
    }
}

void demonstrateCommunicationSystem() {
    std::cout << "\n=== Tank Gauge Communication System Demonstration ===" << std::endl;
    std::cout << "Note: This demo runs without a real server connection." << std::endl;
    
    // Create tank simulator
    TankGaugeSimulator simulator(15000.0f); // 15,000 L tank
    
    // Create communication system (will fail to connect but demonstrate features)
    TankGaugeCommunicationSystem comm_system("TANK_DEMO_002", "127.0.0.1", 8080);
    
    // Set up data callback
    comm_system.setDataRequestCallback([&simulator]() -> TankData {
        return simulator.generateData();
    });
    
    // Set up error callback
    comm_system.setSystemErrorCallback([](const std::string& error) {
        std::cout << "System Error: " << error << std::endl;
    });
    
    std::cout << "\nAttempting system initialization (will fail without server)..." << std::endl;
    
    // Try to initialize (will fail due to no server, but demonstrates the process)
    bool initialized = comm_system.initialize();
    
    if (initialized) {
        std::cout << "✓ Communication system initialized successfully" << std::endl;
        
        // Run for a short time
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        // Get statistics
        auto stats = comm_system.getSystemStatistics();
        std::cout << "\nSystem Statistics:" << std::endl;
        std::cout << "  Messages sent: " << stats.messages_sent << std::endl;
        std::cout << "  Messages received: " << stats.messages_received << std::endl;
        std::cout << "  Bytes transmitted: " << stats.bytes_transmitted << std::endl;
        std::cout << "  Connection attempts: " << stats.connection_attempts << std::endl;
        
        comm_system.shutdown();
    } else {
        std::cout << "✗ Communication system initialization failed (expected without server)" << std::endl;
        std::cout << "  This demonstrates proper error handling and graceful failure" << std::endl;
    }
}

void runContinuousSimulation() {
    std::cout << "\n=== Continuous Tank Gauge Simulation ===" << std::endl;
    std::cout << "Running simulation... Press Ctrl+C to stop" << std::endl;
    
    // Create simulator with leak scenario
    TankGaugeSimulator simulator(12000.0f); // 12,000 L tank
    TankGaugeProtocol protocol("TANK_SIM_001");
    
    int message_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (running.load()) {
        // Generate current tank data
        TankData data = simulator.generateData();
        
        // Create and serialize message
        auto message = protocol.createDataMessage(data);
        auto serialized = message->serialize();
        
        // Display periodic status
        if (++message_count % 10 == 0) {
            float utilization = (data.fuel_level / simulator.getTankCapacity()) * 100.0f;
            
            std::cout << "\rMessage " << message_count 
                      << " | Fuel: " << std::fixed << std::setprecision(1) << data.fuel_level << "L "
                      << "(" << utilization << "%) "
                      << "| Temp: " << data.temperature << "°C "
                      << "| Pressure: " << data.pressure << " kPa    " << std::flush;
            
            // Simulate leak after 30 seconds
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            if (elapsed == 30 && utilization > 50.0f) {
                std::cout << std::endl;
                simulator.simulateLeak(15.0f); // Add 15 L/hour leak
                
                // Send alert
                AlertData alert;
                alert.severity = AlertSeverity::CRITICAL;
                alert.description = "Rapid fuel loss detected - possible leak";
                alert.threshold_value = 5.0f; // Normal consumption
                alert.current_value = 20.0f;  // Elevated consumption with leak
                
                auto alert_msg = protocol.createAlertMessage(alert);
                std::cout << "ALERT SENT: " << alert.description << std::endl;
            }
            
            // Check for low fuel alert
            if (utilization < 20.0f) {
                AlertData low_fuel_alert;
                low_fuel_alert.severity = AlertSeverity::HIGH;
                low_fuel_alert.description = "Low fuel level - refill required";
                low_fuel_alert.threshold_value = simulator.getTankCapacity() * 0.2f;
                low_fuel_alert.current_value = data.fuel_level;
                
                auto alert_msg = protocol.createAlertMessage(low_fuel_alert);
                std::cout << "\nLOW FUEL ALERT: " << low_fuel_alert.description << std::endl;
            }
        }
        
        // Sleep for 1 second (simulating 1Hz data rate)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n\nSimulation completed. Total messages: " << message_count << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "Automated Tank Gauge Communication Protocol Demo" << std::endl;
    std::cout << "================================================" << std::endl;
    
    try {
        // Run different demonstrations
        demonstrateProtocol();
        
        std::cout << "\n\nPress Enter to continue to communication system demo...";
        std::cin.get();
        
        demonstrateCommunicationSystem();
        
        std::cout << "\n\nPress Enter to start continuous simulation (Ctrl+C to stop)...";
        std::cin.get();
        
        runContinuousSimulation();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nDemo completed successfully!" << std::endl;
    return 0;
}