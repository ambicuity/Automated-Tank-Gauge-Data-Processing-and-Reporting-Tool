#include "server_communication.h"
#include "tank_gauge_protocol.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

namespace TankGauge {

// ServerCommunication implementation
ServerCommunication::ServerCommunication(const std::string& server_address, int port)
    : server_address_(server_address), port_(port), socket_fd_(-1),
      connected_(false), should_stop_(false),
      bytes_sent_(0), bytes_received_(0), messages_sent_(0), messages_received_(0),
      heartbeat_interval_(std::chrono::seconds(30)) {
}

ServerCommunication::~ServerCommunication() {
    disconnect();
}

bool ServerCommunication::connect() {
    if (connected_.load()) {
        return true;
    }
    
    if (!setupSocket()) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, server_address_.c_str(), &server_addr.sin_addr) <= 0) {
        cleanupSocket();
        return false;
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cleanupSocket();
        return false;
    }
    
    connected_.store(true);
    should_stop_.store(false);
    
    // Start communication threads
    receive_thread_ = std::make_unique<std::thread>(&ServerCommunication::receiveLoop, this);
    heartbeat_thread_ = std::make_unique<std::thread>(&ServerCommunication::heartbeatLoop, this);
    
    if (connection_callback_) {
        connection_callback_(true);
    }
    
    std::cout << "Connected to server: " << server_address_ << ":" << port_ << std::endl;
    return true;
}

void ServerCommunication::disconnect() {
    if (!connected_.load()) {
        return;
    }
    
    should_stop_.store(true);
    connected_.store(false);
    
    // Wait for threads to finish
    if (receive_thread_ && receive_thread_->joinable()) {
        receive_thread_->join();
    }
    if (heartbeat_thread_ && heartbeat_thread_->joinable()) {
        heartbeat_thread_->join();
    }
    
    cleanupSocket();
    
    if (connection_callback_) {
        connection_callback_(false);
    }
    
    std::cout << "Disconnected from server" << std::endl;
}

bool ServerCommunication::isConnected() const {
    return connected_.load();
}

bool ServerCommunication::sendMessage(const std::vector<uint8_t>& data) {
    if (!connected_.load() || data.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(send_mutex_);
    
    // Send message length first (4 bytes, big-endian)
    uint32_t msg_length = data.size();
    std::vector<uint8_t> length_bytes(4);
    length_bytes[0] = (msg_length >> 24) & 0xFF;
    length_bytes[1] = (msg_length >> 16) & 0xFF;
    length_bytes[2] = (msg_length >> 8) & 0xFF;
    length_bytes[3] = msg_length & 0xFF;
    
    if (!sendRawData(length_bytes)) {
        return false;
    }
    
    // Send actual message data
    if (!sendRawData(data)) {
        return false;
    }
    
    bytes_sent_ += data.size() + 4;
    messages_sent_++;
    
    return true;
}

void ServerCommunication::setMessageReceivedCallback(
    std::function<void(const std::vector<uint8_t>&)> callback) {
    message_callback_ = callback;
}

void ServerCommunication::setConnectionStatusCallback(
    std::function<void(bool)> callback) {
    connection_callback_ = callback;
}

void ServerCommunication::setHeartbeatInterval(std::chrono::seconds interval) {
    heartbeat_interval_ = interval;
}

uint64_t ServerCommunication::getBytesSent() const {
    return bytes_sent_.load();
}

uint64_t ServerCommunication::getBytesReceived() const {
    return bytes_received_.load();
}

uint32_t ServerCommunication::getMessagesSent() const {
    return messages_sent_.load();
}

uint32_t ServerCommunication::getMessagesReceived() const {
    return messages_received_.load();
}

// Private methods
void ServerCommunication::receiveLoop() {
    while (!should_stop_.load() && connected_.load()) {
        try {
            auto message_data = receiveData();
            if (!message_data.empty()) {
                bytes_received_ += message_data.size();
                messages_received_++;
                
                if (message_callback_) {
                    message_callback_(message_data);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Receive error: " << e.what() << std::endl;
            connected_.store(false);
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ServerCommunication::heartbeatLoop() {
    last_heartbeat_sent_ = std::chrono::system_clock::now();
    
    while (!should_stop_.load() && connected_.load()) {
        auto now = std::chrono::system_clock::now();
        auto time_since_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_heartbeat_sent_);
        
        if (time_since_heartbeat >= heartbeat_interval_) {
            // Send heartbeat message (empty message with special header)
            std::vector<uint8_t> heartbeat_msg(1, 0x01); // Simple heartbeat identifier
            if (sendMessage(heartbeat_msg)) {
                last_heartbeat_sent_ = now;
            } else {
                connected_.store(false);
                break;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool ServerCommunication::setupSocket() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    return true;
}

void ServerCommunication::cleanupSocket() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

std::vector<uint8_t> ServerCommunication::receiveData() {
    if (socket_fd_ < 0 || !connected_.load()) {
        return {};
    }
    
    // First, receive message length (4 bytes)
    std::vector<uint8_t> length_bytes(4);
    int bytes_received = 0;
    
    while (bytes_received < 4 && connected_.load()) {
        int result = recv(socket_fd_, &length_bytes[bytes_received], 4 - bytes_received, 0);
        if (result <= 0) {
            if (result == 0) {
                // Connection closed
                connected_.store(false);
            }
            return {};
        }
        bytes_received += result;
    }
    
    // Extract message length
    uint32_t msg_length = (static_cast<uint32_t>(length_bytes[0]) << 24) |
                         (static_cast<uint32_t>(length_bytes[1]) << 16) |
                         (static_cast<uint32_t>(length_bytes[2]) << 8) |
                         static_cast<uint32_t>(length_bytes[3]);
    
    if (msg_length == 0 || msg_length > 65536) { // Reasonable size limit
        return {};
    }
    
    // Receive message data
    std::vector<uint8_t> message_data(msg_length);
    bytes_received = 0;
    
    while (bytes_received < static_cast<int>(msg_length) && connected_.load()) {
        int result = recv(socket_fd_, &message_data[bytes_received], 
                         msg_length - bytes_received, 0);
        if (result <= 0) {
            if (result == 0) {
                connected_.store(false);
            }
            return {};
        }
        bytes_received += result;
    }
    
    return message_data;
}

bool ServerCommunication::sendRawData(const std::vector<uint8_t>& data) {
    if (socket_fd_ < 0 || !connected_.load() || data.empty()) {
        return false;
    }
    
    int bytes_sent = 0;
    int total_bytes = data.size();
    
    while (bytes_sent < total_bytes) {
        int result = send(socket_fd_, &data[bytes_sent], total_bytes - bytes_sent, 0);
        if (result <= 0) {
            connected_.store(false);
            return false;
        }
        bytes_sent += result;
    }
    
    return true;
}

// TankGaugeCommunicationSystem implementation
TankGaugeCommunicationSystem::TankGaugeCommunicationSystem(
    const std::string& tank_id, const std::string& server_address, int port)
    : tank_id_(tank_id), operational_(false), should_stop_(false),
      data_transmission_interval_(std::chrono::seconds(60)),
      connection_retry_interval_(std::chrono::seconds(30)),
      max_connection_retries_(5) {
    
    protocol_ = std::make_unique<TankGaugeProtocol>(tank_id);
    server_comm_ = std::make_unique<ServerCommunication>(server_address, port);
    
    system_start_time_ = std::chrono::system_clock::now();
    
    // Initialize statistics
    memset(&stats_, 0, sizeof(stats_));
    stats_.system_start_time = system_start_time_;
}

TankGaugeCommunicationSystem::~TankGaugeCommunicationSystem() {
    shutdown();
}

bool TankGaugeCommunicationSystem::initialize() {
    if (operational_.load()) {
        return true;
    }
    
    // Set up protocol callbacks
    server_comm_->setMessageReceivedCallback(
        [this](const std::vector<uint8_t>& data) {
            handleProtocolMessage(data);
        });
    
    server_comm_->setConnectionStatusCallback(
        [this](bool connected) {
            handleConnectionStatus(connected);
        });
    
    // Attempt initial connection
    if (!attemptServerConnection()) {
        return false;
    }
    
    operational_.store(true);
    should_stop_.store(false);
    
    // Start system threads
    data_transmission_thread_ = std::make_unique<std::thread>(
        &TankGaugeCommunicationSystem::dataTransmissionLoop, this);
    connection_monitor_thread_ = std::make_unique<std::thread>(
        &TankGaugeCommunicationSystem::connectionMonitorLoop, this);
    
    std::cout << "Tank Gauge Communication System initialized for: " << tank_id_ << std::endl;
    return true;
}

void TankGaugeCommunicationSystem::shutdown() {
    if (!operational_.load()) {
        return;
    }
    
    should_stop_.store(true);
    operational_.store(false);
    
    // Wait for threads to finish
    if (data_transmission_thread_ && data_transmission_thread_->joinable()) {
        data_transmission_thread_->join();
    }
    if (connection_monitor_thread_ && connection_monitor_thread_->joinable()) {
        connection_monitor_thread_->join();
    }
    
    // Disconnect from server
    server_comm_->disconnect();
    
    std::cout << "Tank Gauge Communication System shutdown complete" << std::endl;
}

bool TankGaugeCommunicationSystem::isOperational() const {
    return operational_.load() && server_comm_->isConnected();
}

bool TankGaugeCommunicationSystem::sendTankData(float fuel_level, float temperature, 
                                               float pressure, float flow_rate) {
    if (!isOperational()) {
        return false;
    }
    
    TankData data;
    data.fuel_level = fuel_level;
    data.temperature = temperature;
    data.pressure = pressure;
    data.flow_rate = flow_rate;
    
    auto message = protocol_->createDataMessage(data);
    auto serialized = message->serialize();
    
    bool success = server_comm_->sendMessage(serialized);
    if (success) {
        last_data_transmission_ = std::chrono::system_clock::now();
        updateStatistics();
    } else {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.failed_transmissions++;
    }
    
    return success;
}

bool TankGaugeCommunicationSystem::sendAlert(AlertSeverity severity, 
                                           const std::string& description, 
                                           float threshold_value, float current_value) {
    if (!isOperational()) {
        return false;
    }
    
    AlertData alert;
    alert.severity = severity;
    alert.description = description;
    alert.threshold_value = threshold_value;
    alert.current_value = current_value;
    
    auto message = protocol_->createAlertMessage(alert);
    auto serialized = message->serialize();
    
    return server_comm_->sendMessage(serialized);
}

// Configuration methods
void TankGaugeCommunicationSystem::setDataTransmissionInterval(std::chrono::seconds interval) {
    data_transmission_interval_ = interval;
}

void TankGaugeCommunicationSystem::setConnectionRetryInterval(std::chrono::seconds interval) {
    connection_retry_interval_ = interval;
}

void TankGaugeCommunicationSystem::setMaxConnectionRetries(int max_retries) {
    max_connection_retries_ = max_retries;
}

// Status monitoring
bool TankGaugeCommunicationSystem::isConnectedToServer() const {
    return server_comm_->isConnected();
}

std::chrono::system_clock::time_point TankGaugeCommunicationSystem::getLastDataTransmission() const {
    return last_data_transmission_;
}

std::chrono::system_clock::time_point TankGaugeCommunicationSystem::getLastServerContact() const {
    return last_server_contact_;
}

TankGaugeCommunicationSystem::SystemStats TankGaugeCommunicationSystem::getSystemStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

// Event callbacks
void TankGaugeCommunicationSystem::setDataRequestCallback(std::function<TankData()> callback) {
    data_request_callback_ = callback;
}

void TankGaugeCommunicationSystem::setCommandReceivedCallback(
    std::function<void(const std::string&)> callback) {
    command_callback_ = callback;
}

void TankGaugeCommunicationSystem::setSystemErrorCallback(
    std::function<void(const std::string&)> callback) {
    error_callback_ = callback;
}

// Private methods
void TankGaugeCommunicationSystem::dataTransmissionLoop() {
    while (!should_stop_.load()) {
        if (isOperational() && data_request_callback_) {
            try {
                TankData data = data_request_callback_();
                sendTankData(data.fuel_level, data.temperature, data.pressure, data.flow_rate);
            } catch (const std::exception& e) {
                handleSystemError("Data transmission error: " + std::string(e.what()));
            }
        }
        
        std::this_thread::sleep_for(data_transmission_interval_);
    }
}

void TankGaugeCommunicationSystem::connectionMonitorLoop() {
    int retry_count = 0;
    
    while (!should_stop_.load()) {
        if (!server_comm_->isConnected() && operational_.load()) {
            if (retry_count < max_connection_retries_) {
                std::cout << "Attempting to reconnect... (attempt " << (retry_count + 1) 
                         << "/" << max_connection_retries_ << ")" << std::endl;
                
                if (attemptServerConnection()) {
                    retry_count = 0;
                } else {
                    retry_count++;
                }
            } else {
                handleSystemError("Max connection retries exceeded. System offline.");
                operational_.store(false);
                break;
            }
        }
        
        std::this_thread::sleep_for(connection_retry_interval_);
    }
}

void TankGaugeCommunicationSystem::handleProtocolMessage(const std::vector<uint8_t>& data) {
    if (protocol_->processReceivedMessage(data)) {
        last_server_contact_ = std::chrono::system_clock::now();
        updateStatistics();
    }
}

void TankGaugeCommunicationSystem::handleConnectionStatus(bool connected) {
    if (connected) {
        std::cout << "Successfully connected to server for " << tank_id_ << std::endl;
    } else {
        std::cout << "Lost connection to server for " << tank_id_ << std::endl;
    }
}

void TankGaugeCommunicationSystem::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.messages_sent = server_comm_->getMessagesSent();
    stats_.messages_received = server_comm_->getMessagesReceived();
    stats_.bytes_transmitted = server_comm_->getBytesSent() + server_comm_->getBytesReceived();
}

void TankGaugeCommunicationSystem::logError(const std::string& error) {
    std::cerr << "[" << tank_id_ << "] ERROR: " << error << std::endl;
}

bool TankGaugeCommunicationSystem::attemptServerConnection() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.connection_attempts++;
    
    return server_comm_->connect();
}

void TankGaugeCommunicationSystem::handleSystemError(const std::string& error) {
    logError(error);
    if (error_callback_) {
        error_callback_(error);
    }
}

} // namespace TankGauge