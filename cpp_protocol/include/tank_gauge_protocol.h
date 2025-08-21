#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <queue>
#include <map>
#include <functional>

namespace TankGauge {

// Message types for communication protocol
enum class MessageType : uint8_t {
    HEARTBEAT = 0x01,
    DATA_TRANSMISSION = 0x02,
    ALERT = 0x03,
    COMMAND_REQUEST = 0x04,
    COMMAND_RESPONSE = 0x05,
    ERROR = 0xFF
};

// Alert severity levels
enum class AlertSeverity : uint8_t {
    LOW = 0x01,
    MEDIUM = 0x02,
    HIGH = 0x03,
    CRITICAL = 0x04
};

// Protocol message structure
struct Message {
    uint32_t message_id;
    MessageType type;
    std::string tank_id;
    std::chrono::system_clock::time_point timestamp;
    std::vector<uint8_t> payload;
    uint32_t checksum;
    
    Message();
    Message(MessageType type, const std::string& tank_id, const std::vector<uint8_t>& data);
    
    // Serialization methods
    std::vector<uint8_t> serialize() const;
    static std::unique_ptr<Message> deserialize(const std::vector<uint8_t>& data);
    
    // Validation
    bool validate() const;
    void calculateChecksum();
    
private:
    static uint32_t next_message_id_;
    uint32_t crc32(const std::vector<uint8_t>& data) const;
};

// Tank gauge data structure
struct TankData {
    float fuel_level;      // Liters
    float temperature;     // Celsius
    float pressure;        // kPa
    float flow_rate;       // L/min
    
    std::vector<uint8_t> serialize() const;
    static TankData deserialize(const std::vector<uint8_t>& data);
};

// Alert data structure
struct AlertData {
    AlertSeverity severity;
    std::string description;
    float threshold_value;
    float current_value;
    
    std::vector<uint8_t> serialize() const;
    static AlertData deserialize(const std::vector<uint8_t>& data);
};

// Communication protocol handler
class TankGaugeProtocol {
public:
    TankGaugeProtocol(const std::string& tank_id);
    ~TankGaugeProtocol() = default;
    
    // Message creation methods
    std::unique_ptr<Message> createHeartbeatMessage();
    std::unique_ptr<Message> createDataMessage(const TankData& data);
    std::unique_ptr<Message> createAlertMessage(const AlertData& alert);
    std::unique_ptr<Message> createCommandResponse(uint32_t request_id, 
                                                  const std::vector<uint8_t>& response);
    std::unique_ptr<Message> createErrorMessage(const std::string& error_description);
    
    // Message processing
    bool processReceivedMessage(const std::vector<uint8_t>& raw_data);
    
    // Protocol settings
    void setHeartbeatInterval(std::chrono::seconds interval);
    void setMaxRetransmissions(int max_retries);
    void setResponseTimeout(std::chrono::seconds timeout);
    
    // Status queries
    bool isHealthy() const;
    std::chrono::system_clock::time_point getLastHeartbeat() const;
    int getPendingMessages() const;
    
    // Callbacks for received messages
    void setDataReceivedCallback(std::function<void(const TankData&)> callback);
    void setCommandReceivedCallback(std::function<void(uint32_t, const std::vector<uint8_t>&)> callback);
    void setAlertReceivedCallback(std::function<void(const AlertData&)> callback);
    
private:
    std::string tank_id_;
    std::chrono::seconds heartbeat_interval_;
    std::chrono::seconds response_timeout_;
    int max_retransmissions_;
    
    std::chrono::system_clock::time_point last_heartbeat_;
    std::queue<std::unique_ptr<Message>> pending_messages_;
    std::map<uint32_t, std::chrono::system_clock::time_point> awaiting_responses_;
    
    // Callbacks
    std::function<void(const TankData&)> data_callback_;
    std::function<void(uint32_t, const std::vector<uint8_t>&)> command_callback_;
    std::function<void(const AlertData&)> alert_callback_;
    
    // Internal methods
    void processHeartbeat(const Message& msg);
    void processDataTransmission(const Message& msg);
    void processAlert(const Message& msg);
    void processCommandRequest(const Message& msg);
    void processCommandResponse(const Message& msg);
    void processError(const Message& msg);
    
    bool validateMessage(const Message& msg);
    void updateLastHeartbeat();
};

} // namespace TankGauge