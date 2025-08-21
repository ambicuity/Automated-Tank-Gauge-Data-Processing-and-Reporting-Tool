#include "tank_gauge_protocol.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <queue>
#include <map>
#include <functional>

namespace TankGauge {

// Static member initialization
uint32_t Message::next_message_id_ = 1;

// Message implementation
Message::Message() : message_id(0), type(MessageType::HEARTBEAT), checksum(0) {
    timestamp = std::chrono::system_clock::now();
}

Message::Message(MessageType type, const std::string& tank_id, const std::vector<uint8_t>& data)
    : message_id(next_message_id_++), type(type), tank_id(tank_id), payload(data), checksum(0) {
    timestamp = std::chrono::system_clock::now();
    calculateChecksum();
}

std::vector<uint8_t> Message::serialize() const {
    std::vector<uint8_t> data;
    
    // Message ID (4 bytes)
    data.push_back((message_id >> 24) & 0xFF);
    data.push_back((message_id >> 16) & 0xFF);
    data.push_back((message_id >> 8) & 0xFF);
    data.push_back(message_id & 0xFF);
    
    // Message type (1 byte)
    data.push_back(static_cast<uint8_t>(type));
    
    // Tank ID length (1 byte) + Tank ID
    data.push_back(static_cast<uint8_t>(tank_id.length()));
    for (char c : tank_id) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // Timestamp (8 bytes - Unix timestamp in milliseconds)
    auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    
    for (int i = 7; i >= 0; i--) {
        data.push_back((time_ms >> (i * 8)) & 0xFF);
    }
    
    // Payload length (4 bytes)
    uint32_t payload_size = payload.size();
    data.push_back((payload_size >> 24) & 0xFF);
    data.push_back((payload_size >> 16) & 0xFF);
    data.push_back((payload_size >> 8) & 0xFF);
    data.push_back(payload_size & 0xFF);
    
    // Payload
    data.insert(data.end(), payload.begin(), payload.end());
    
    // Checksum (4 bytes)
    data.push_back((checksum >> 24) & 0xFF);
    data.push_back((checksum >> 16) & 0xFF);
    data.push_back((checksum >> 8) & 0xFF);
    data.push_back(checksum & 0xFF);
    
    return data;
}

std::unique_ptr<Message> Message::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 22) { // Minimum message size
        return nullptr;
    }
    
    auto msg = std::make_unique<Message>();
    size_t offset = 0;
    
    // Message ID
    msg->message_id = (static_cast<uint32_t>(data[offset]) << 24) |
                     (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) |
                     static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Message type
    msg->type = static_cast<MessageType>(data[offset++]);
    
    // Tank ID
    uint8_t tank_id_len = data[offset++];
    if (offset + tank_id_len >= data.size()) return nullptr;
    
    msg->tank_id = std::string(reinterpret_cast<const char*>(&data[offset]), tank_id_len);
    offset += tank_id_len;
    
    // Timestamp
    uint64_t time_ms = 0;
    for (int i = 0; i < 8; i++) {
        time_ms = (time_ms << 8) | data[offset + i];
    }
    offset += 8;
    
    msg->timestamp = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(time_ms));
    
    // Payload length
    uint32_t payload_size = (static_cast<uint32_t>(data[offset]) << 24) |
                           (static_cast<uint32_t>(data[offset + 1]) << 16) |
                           (static_cast<uint32_t>(data[offset + 2]) << 8) |
                           static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Payload
    if (offset + payload_size + 4 > data.size()) return nullptr;
    
    msg->payload = std::vector<uint8_t>(data.begin() + offset, 
                                       data.begin() + offset + payload_size);
    offset += payload_size;
    
    // Checksum
    msg->checksum = (static_cast<uint32_t>(data[offset]) << 24) |
                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                   static_cast<uint32_t>(data[offset + 3]);
    
    // Validate the message before returning
    if (!msg->validate()) {
        return nullptr;
    }
    
    return msg;
}

bool Message::validate() const {
    // Create a copy and recalculate checksum
    Message temp = *this;
    temp.calculateChecksum();
    return temp.checksum == checksum;
}

void Message::calculateChecksum() {
    std::vector<uint8_t> data_for_checksum;
    
    // Include all data except checksum in calculation
    data_for_checksum.push_back((message_id >> 24) & 0xFF);
    data_for_checksum.push_back((message_id >> 16) & 0xFF);
    data_for_checksum.push_back((message_id >> 8) & 0xFF);
    data_for_checksum.push_back(message_id & 0xFF);
    
    data_for_checksum.push_back(static_cast<uint8_t>(type));
    data_for_checksum.push_back(static_cast<uint8_t>(tank_id.length()));
    
    for (char c : tank_id) {
        data_for_checksum.push_back(static_cast<uint8_t>(c));
    }
    
    data_for_checksum.insert(data_for_checksum.end(), payload.begin(), payload.end());
    
    checksum = crc32(data_for_checksum);
}

uint32_t Message::crc32(const std::vector<uint8_t>& data) const {
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return ~crc;
}

// TankData implementation
std::vector<uint8_t> TankData::serialize() const {
    std::vector<uint8_t> data(16); // 4 floats * 4 bytes each
    
    // Convert floats to bytes (little-endian)
    memcpy(&data[0], &fuel_level, 4);
    memcpy(&data[4], &temperature, 4);
    memcpy(&data[8], &pressure, 4);
    memcpy(&data[12], &flow_rate, 4);
    
    return data;
}

TankData TankData::deserialize(const std::vector<uint8_t>& data) {
    TankData tank_data;
    
    if (data.size() >= 16) {
        memcpy(&tank_data.fuel_level, &data[0], 4);
        memcpy(&tank_data.temperature, &data[4], 4);
        memcpy(&tank_data.pressure, &data[8], 4);
        memcpy(&tank_data.flow_rate, &data[12], 4);
    }
    
    return tank_data;
}

// AlertData implementation
std::vector<uint8_t> AlertData::serialize() const {
    std::vector<uint8_t> data;
    
    // Severity (1 byte)
    data.push_back(static_cast<uint8_t>(severity));
    
    // Description length + description
    data.push_back(static_cast<uint8_t>(description.length()));
    for (char c : description) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // Threshold value (4 bytes)
    uint8_t* threshold_bytes = reinterpret_cast<uint8_t*>(
        const_cast<float*>(&threshold_value));
    for (int i = 0; i < 4; i++) {
        data.push_back(threshold_bytes[i]);
    }
    
    // Current value (4 bytes)
    uint8_t* current_bytes = reinterpret_cast<uint8_t*>(
        const_cast<float*>(&current_value));
    for (int i = 0; i < 4; i++) {
        data.push_back(current_bytes[i]);
    }
    
    return data;
}

AlertData AlertData::deserialize(const std::vector<uint8_t>& data) {
    AlertData alert;
    
    if (data.size() < 10) return alert; // Minimum size check
    
    size_t offset = 0;
    
    // Severity
    alert.severity = static_cast<AlertSeverity>(data[offset++]);
    
    // Description
    uint8_t desc_len = data[offset++];
    if (offset + desc_len + 8 <= data.size()) {
        alert.description = std::string(reinterpret_cast<const char*>(&data[offset]), desc_len);
        offset += desc_len;
        
        // Threshold value
        memcpy(&alert.threshold_value, &data[offset], 4);
        offset += 4;
        
        // Current value
        memcpy(&alert.current_value, &data[offset], 4);
    }
    
    return alert;
}

// TankGaugeProtocol implementation
TankGaugeProtocol::TankGaugeProtocol(const std::string& tank_id)
    : tank_id_(tank_id), 
      heartbeat_interval_(std::chrono::seconds(30)),
      response_timeout_(std::chrono::seconds(10)),
      max_retransmissions_(3) {
    updateLastHeartbeat();
}

std::unique_ptr<Message> TankGaugeProtocol::createHeartbeatMessage() {
    std::vector<uint8_t> empty_payload;
    updateLastHeartbeat();
    return std::make_unique<Message>(MessageType::HEARTBEAT, tank_id_, empty_payload);
}

std::unique_ptr<Message> TankGaugeProtocol::createDataMessage(const TankData& data) {
    auto payload = data.serialize();
    return std::make_unique<Message>(MessageType::DATA_TRANSMISSION, tank_id_, payload);
}

std::unique_ptr<Message> TankGaugeProtocol::createAlertMessage(const AlertData& alert) {
    auto payload = alert.serialize();
    return std::make_unique<Message>(MessageType::ALERT, tank_id_, payload);
}

std::unique_ptr<Message> TankGaugeProtocol::createCommandResponse(uint32_t request_id, 
                                                                 const std::vector<uint8_t>& response) {
    std::vector<uint8_t> payload;
    
    // Add request ID to payload
    payload.push_back((request_id >> 24) & 0xFF);
    payload.push_back((request_id >> 16) & 0xFF);
    payload.push_back((request_id >> 8) & 0xFF);
    payload.push_back(request_id & 0xFF);
    
    // Add response data
    payload.insert(payload.end(), response.begin(), response.end());
    
    return std::make_unique<Message>(MessageType::COMMAND_RESPONSE, tank_id_, payload);
}

std::unique_ptr<Message> TankGaugeProtocol::createErrorMessage(const std::string& error_description) {
    std::vector<uint8_t> payload(error_description.begin(), error_description.end());
    return std::make_unique<Message>(MessageType::ERROR, tank_id_, payload);
}

bool TankGaugeProtocol::processReceivedMessage(const std::vector<uint8_t>& raw_data) {
    auto message = Message::deserialize(raw_data);
    if (!message || !validateMessage(*message)) {
        return false;
    }
    
    switch (message->type) {
        case MessageType::HEARTBEAT:
            processHeartbeat(*message);
            break;
        case MessageType::DATA_TRANSMISSION:
            processDataTransmission(*message);
            break;
        case MessageType::ALERT:
            processAlert(*message);
            break;
        case MessageType::COMMAND_REQUEST:
            processCommandRequest(*message);
            break;
        case MessageType::COMMAND_RESPONSE:
            processCommandResponse(*message);
            break;
        case MessageType::ERROR:
            processError(*message);
            break;
        default:
            return false;
    }
    
    return true;
}

// Protocol settings
void TankGaugeProtocol::setHeartbeatInterval(std::chrono::seconds interval) {
    heartbeat_interval_ = interval;
}

void TankGaugeProtocol::setMaxRetransmissions(int max_retries) {
    max_retransmissions_ = max_retries;
}

void TankGaugeProtocol::setResponseTimeout(std::chrono::seconds timeout) {
    response_timeout_ = timeout;
}

// Status queries
bool TankGaugeProtocol::isHealthy() const {
    auto now = std::chrono::system_clock::now();
    auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_heartbeat_);
    return time_since_heartbeat <= heartbeat_interval_ * 2; // Allow 2x interval
}

std::chrono::system_clock::time_point TankGaugeProtocol::getLastHeartbeat() const {
    return last_heartbeat_;
}

int TankGaugeProtocol::getPendingMessages() const {
    return pending_messages_.size();
}

// Callbacks
void TankGaugeProtocol::setDataReceivedCallback(std::function<void(const TankData&)> callback) {
    data_callback_ = callback;
}

void TankGaugeProtocol::setCommandReceivedCallback(
    std::function<void(uint32_t, const std::vector<uint8_t>&)> callback) {
    command_callback_ = callback;
}

void TankGaugeProtocol::setAlertReceivedCallback(std::function<void(const AlertData&)> callback) {
    alert_callback_ = callback;
}

// Private methods
void TankGaugeProtocol::processHeartbeat(const Message& msg) {
    updateLastHeartbeat();
}

void TankGaugeProtocol::processDataTransmission(const Message& msg) {
    if (data_callback_) {
        TankData data = TankData::deserialize(msg.payload);
        data_callback_(data);
    }
}

void TankGaugeProtocol::processAlert(const Message& msg) {
    if (alert_callback_) {
        AlertData alert = AlertData::deserialize(msg.payload);
        alert_callback_(alert);
    }
}

void TankGaugeProtocol::processCommandRequest(const Message& msg) {
    if (command_callback_ && msg.payload.size() >= 4) {
        uint32_t request_id = (static_cast<uint32_t>(msg.payload[0]) << 24) |
                             (static_cast<uint32_t>(msg.payload[1]) << 16) |
                             (static_cast<uint32_t>(msg.payload[2]) << 8) |
                             static_cast<uint32_t>(msg.payload[3]);
        
        std::vector<uint8_t> command_data(msg.payload.begin() + 4, msg.payload.end());
        command_callback_(request_id, command_data);
        
        awaiting_responses_[request_id] = std::chrono::system_clock::now();
    }
}

void TankGaugeProtocol::processCommandResponse(const Message& msg) {
    if (msg.payload.size() >= 4) {
        uint32_t request_id = (static_cast<uint32_t>(msg.payload[0]) << 24) |
                             (static_cast<uint32_t>(msg.payload[1]) << 16) |
                             (static_cast<uint32_t>(msg.payload[2]) << 8) |
                             static_cast<uint32_t>(msg.payload[3]);
        
        awaiting_responses_.erase(request_id);
    }
}

void TankGaugeProtocol::processError(const Message& msg) {
    std::string error_desc(msg.payload.begin(), msg.payload.end());
    std::cerr << "Protocol error from " << msg.tank_id << ": " << error_desc << std::endl;
}

bool TankGaugeProtocol::validateMessage(const Message& msg) {
    return msg.validate() && !msg.tank_id.empty();
}

void TankGaugeProtocol::updateLastHeartbeat() {
    last_heartbeat_ = std::chrono::system_clock::now();
}

} // namespace TankGauge