#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace TankGauge {

class TankGaugeProtocol;
struct TankData;
enum class AlertSeverity : uint8_t;

// Network communication interface
class ServerCommunication {
public:
    ServerCommunication(const std::string& server_address, int port);
    ~ServerCommunication();
    
    // Connection management
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Message transmission
    bool sendMessage(const std::vector<uint8_t>& data);
    void setMessageReceivedCallback(std::function<void(const std::vector<uint8_t>&)> callback);
    
    // Connection monitoring
    void setConnectionStatusCallback(std::function<void(bool)> callback);
    void setHeartbeatInterval(std::chrono::seconds interval);
    
    // Statistics
    uint64_t getBytesSent() const;
    uint64_t getBytesReceived() const;
    uint32_t getMessagesSent() const;
    uint32_t getMessagesReceived() const;
    
private:
    std::string server_address_;
    int port_;
    int socket_fd_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> should_stop_;
    
    std::unique_ptr<std::thread> receive_thread_;
    std::unique_ptr<std::thread> heartbeat_thread_;
    
    std::mutex send_mutex_;
    
    // Statistics
    std::atomic<uint64_t> bytes_sent_;
    std::atomic<uint64_t> bytes_received_;
    std::atomic<uint32_t> messages_sent_;
    std::atomic<uint32_t> messages_received_;
    
    std::chrono::seconds heartbeat_interval_;
    std::chrono::system_clock::time_point last_heartbeat_sent_;
    
    // Callbacks
    std::function<void(const std::vector<uint8_t>&)> message_callback_;
    std::function<void(bool)> connection_callback_;
    
    // Internal methods
    void receiveLoop();
    void heartbeatLoop();
    bool setupSocket();
    void cleanupSocket();
    
    std::vector<uint8_t> receiveData();
    bool sendRawData(const std::vector<uint8_t>& data);
};

// Complete tank gauge communication system
class TankGaugeCommunicationSystem {
public:
    TankGaugeCommunicationSystem(const std::string& tank_id, 
                                const std::string& server_address, 
                                int port);
    ~TankGaugeCommunicationSystem();
    
    // System control
    bool initialize();
    void shutdown();
    bool isOperational() const;
    
    // Data transmission
    bool sendTankData(float fuel_level, float temperature, float pressure, float flow_rate);
    bool sendAlert(AlertSeverity severity, const std::string& description, 
                  float threshold_value, float current_value);
    
    // Configuration
    void setDataTransmissionInterval(std::chrono::seconds interval);
    void setConnectionRetryInterval(std::chrono::seconds interval);
    void setMaxConnectionRetries(int max_retries);
    
    // Status monitoring
    bool isConnectedToServer() const;
    std::chrono::system_clock::time_point getLastDataTransmission() const;
    std::chrono::system_clock::time_point getLastServerContact() const;
    
    // Statistics
    struct SystemStats {
        uint32_t messages_sent;
        uint32_t messages_received;
        uint64_t bytes_transmitted;
        uint64_t connection_attempts;
        uint64_t failed_transmissions;
        std::chrono::system_clock::time_point system_start_time;
    };
    
    SystemStats getSystemStatistics() const;
    
    // Event callbacks
    void setDataRequestCallback(std::function<TankData()> callback);
    void setCommandReceivedCallback(std::function<void(const std::string&)> callback);
    void setSystemErrorCallback(std::function<void(const std::string&)> callback);
    
private:
    std::unique_ptr<TankGaugeProtocol> protocol_;
    std::unique_ptr<ServerCommunication> server_comm_;
    
    std::string tank_id_;
    
    std::atomic<bool> operational_;
    std::atomic<bool> should_stop_;
    
    std::unique_ptr<std::thread> data_transmission_thread_;
    std::unique_ptr<std::thread> connection_monitor_thread_;
    
    std::chrono::seconds data_transmission_interval_;
    std::chrono::seconds connection_retry_interval_;
    int max_connection_retries_;
    
    std::chrono::system_clock::time_point last_data_transmission_;
    std::chrono::system_clock::time_point last_server_contact_;
    std::chrono::system_clock::time_point system_start_time_;
    
    mutable std::mutex stats_mutex_;
    SystemStats stats_;
    
    // Callbacks
    std::function<TankData()> data_request_callback_;
    std::function<void(const std::string&)> command_callback_;
    std::function<void(const std::string&)> error_callback_;
    
    // Internal methods
    void dataTransmissionLoop();
    void connectionMonitorLoop();
    
    void handleProtocolMessage(const std::vector<uint8_t>& data);
    void handleConnectionStatus(bool connected);
    
    void updateStatistics();
    void logError(const std::string& error);
    
    bool attemptServerConnection();
    void handleSystemError(const std::string& error);
};

} // namespace TankGauge