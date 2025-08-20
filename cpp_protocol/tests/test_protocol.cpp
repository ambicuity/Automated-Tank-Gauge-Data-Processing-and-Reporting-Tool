#include "tank_gauge_protocol.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

using namespace TankGauge;

void testMessageSerialization() {
    std::cout << "Testing message serialization..." << std::endl;
    
    // Create test data
    std::vector<uint8_t> test_payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    // Create message
    Message msg(MessageType::DATA_TRANSMISSION, "TEST_TANK", test_payload);
    
    // Serialize
    auto serialized = msg.serialize();
    assert(!serialized.empty());
    
    // Deserialize
    auto deserialized = Message::deserialize(serialized);
    assert(deserialized != nullptr);
    assert(deserialized->validate());
    
    // Check fields
    assert(deserialized->type == MessageType::DATA_TRANSMISSION);
    assert(deserialized->tank_id == "TEST_TANK");
    assert(deserialized->payload == test_payload);
    
    std::cout << "✓ Message serialization test passed" << std::endl;
}

void testTankDataSerialization() {
    std::cout << "Testing tank data serialization..." << std::endl;
    
    TankData original;
    original.fuel_level = 1234.5f;
    original.temperature = 25.3f;
    original.pressure = 101.7f;
    original.flow_rate = 2.1f;
    
    // Serialize
    auto serialized = original.serialize();
    assert(serialized.size() == 16); // 4 floats * 4 bytes each
    
    // Deserialize
    TankData recovered = TankData::deserialize(serialized);
    
    // Check values (allowing for floating point precision)
    assert(std::abs(recovered.fuel_level - original.fuel_level) < 0.001f);
    assert(std::abs(recovered.temperature - original.temperature) < 0.001f);
    assert(std::abs(recovered.pressure - original.pressure) < 0.001f);
    assert(std::abs(recovered.flow_rate - original.flow_rate) < 0.001f);
    
    std::cout << "✓ Tank data serialization test passed" << std::endl;
}

void testAlertDataSerialization() {
    std::cout << "Testing alert data serialization..." << std::endl;
    
    AlertData original;
    original.severity = AlertSeverity::HIGH;
    original.description = "Test alert message";
    original.threshold_value = 100.0f;
    original.current_value = 150.0f;
    
    // Serialize
    auto serialized = original.serialize();
    assert(!serialized.empty());
    
    // Deserialize
    AlertData recovered = AlertData::deserialize(serialized);
    
    // Check values
    assert(recovered.severity == original.severity);
    assert(recovered.description == original.description);
    assert(std::abs(recovered.threshold_value - original.threshold_value) < 0.001f);
    assert(std::abs(recovered.current_value - original.current_value) < 0.001f);
    
    std::cout << "✓ Alert data serialization test passed" << std::endl;
}

void testProtocolMessageCreation() {
    std::cout << "Testing protocol message creation..." << std::endl;
    
    TankGaugeProtocol protocol("TEST_TANK_001");
    
    // Test heartbeat message
    auto heartbeat = protocol.createHeartbeatMessage();
    assert(heartbeat != nullptr);
    assert(heartbeat->type == MessageType::HEARTBEAT);
    assert(heartbeat->tank_id == "TEST_TANK_001");
    assert(heartbeat->payload.empty());
    
    // Test data message
    TankData test_data;
    test_data.fuel_level = 5000.0f;
    test_data.temperature = 20.0f;
    test_data.pressure = 101.3f;
    test_data.flow_rate = 1.5f;
    
    auto data_msg = protocol.createDataMessage(test_data);
    assert(data_msg != nullptr);
    assert(data_msg->type == MessageType::DATA_TRANSMISSION);
    assert(data_msg->tank_id == "TEST_TANK_001");
    assert(data_msg->payload.size() == 16); // 4 floats
    
    // Test alert message
    AlertData test_alert;
    test_alert.severity = AlertSeverity::CRITICAL;
    test_alert.description = "Critical system failure";
    test_alert.threshold_value = 10.0f;
    test_alert.current_value = 5.0f;
    
    auto alert_msg = protocol.createAlertMessage(test_alert);
    assert(alert_msg != nullptr);
    assert(alert_msg->type == MessageType::ALERT);
    assert(alert_msg->tank_id == "TEST_TANK_001");
    assert(!alert_msg->payload.empty());
    
    // Test command response
    std::vector<uint8_t> response_data = {0xAA, 0xBB, 0xCC, 0xDD};
    auto cmd_response = protocol.createCommandResponse(12345, response_data);
    assert(cmd_response != nullptr);
    assert(cmd_response->type == MessageType::COMMAND_RESPONSE);
    assert(cmd_response->payload.size() == 4 + response_data.size()); // ID + data
    
    std::cout << "✓ Protocol message creation test passed" << std::endl;
}

void testMessageValidation() {
    std::cout << "Testing message validation..." << std::endl;
    
    // Create valid message
    std::vector<uint8_t> test_payload = {0x11, 0x22, 0x33};
    Message valid_msg(MessageType::DATA_TRANSMISSION, "VALID_TANK", test_payload);
    
    assert(valid_msg.validate());
    std::cout << "✓ Valid message validation passed" << std::endl;
    
    // Test with corrupted data
    auto serialized = valid_msg.serialize();
    
    // Corrupt multiple bytes to ensure checksum failure
    serialized[serialized.size() - 1] ^= 0xFF; // Corrupt last byte of checksum
    serialized[serialized.size() - 2] ^= 0xFF; // Corrupt second-to-last byte
    
    auto corrupted = Message::deserialize(serialized);
    // Should fail validation due to checksum mismatch
    assert(corrupted == nullptr);
    
    std::cout << "✓ Corrupted message validation test passed" << std::endl;
}

void testProtocolSettings() {
    std::cout << "Testing protocol settings..." << std::endl;
    
    TankGaugeProtocol protocol("SETTINGS_TEST");
    
    // Test default settings
    assert(protocol.isHealthy()); // Should be healthy initially
    assert(protocol.getPendingMessages() == 0);
    
    // Test setting changes
    protocol.setHeartbeatInterval(std::chrono::seconds(60));
    protocol.setMaxRetransmissions(5);
    protocol.setResponseTimeout(std::chrono::seconds(15));
    
    std::cout << "✓ Protocol settings test passed" << std::endl;
}

void runAllTests() {
    std::cout << "=== Tank Gauge Protocol Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        testMessageSerialization();
        testTankDataSerialization();
        testAlertDataSerialization();
        testProtocolMessageCreation();
        testMessageValidation();
        testProtocolSettings();
        
        std::cout << std::endl;
        std::cout << "=== All Tests Passed! ===" << std::endl;
        std::cout << "✓ Message serialization and deserialization" << std::endl;
        std::cout << "✓ Tank data handling" << std::endl;
        std::cout << "✓ Alert data handling" << std::endl;
        std::cout << "✓ Protocol message creation" << std::endl;
        std::cout << "✓ Message validation and error handling" << std::endl;
        std::cout << "✓ Protocol configuration" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    try {
        runAllTests();
        return 0;
    } catch (...) {
        std::cerr << "Tests failed!" << std::endl;
        return 1;
    }
}