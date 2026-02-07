#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

/**
 * @brief WiFi manager with captive portal support
 * 
 * Handles WiFi connectivity with automatic fallback to AP mode
 * when station connection fails. Implements captive portal for
 * easy phone-based configuration.
 */
class WiFiManager {
public:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED_STATION,
        AP_MODE
    };
    
    /**
     * @brief Initialize WiFi manager
     */
    void begin();
    
    /**
     * @brief Process WiFi events and DNS (call in loop)
     */
    void loop();
    
    /**
     * @brief Start AP mode for configuration
     * @param apName Name of the access point
     */
    void startAP(const String& apName);
    
    /**
     * @brief Connect to configured WiFi network
     * @param ssid Network SSID
     * @param password Network password
     * @return true if connection started
     */
    bool connectStation(const char* ssid, const char* password);
    
    /**
     * @brief Disconnect and restart connection process
     */
    void reconnect();
    
    /**
     * @brief Get current connection state
     * @return Current WiFi state
     */
    State getState() const;
    
    /**
     * @brief Get current IP address
     * @return IP as string
     */
    String getIP() const;
    
    /**
     * @brief Get WiFi signal strength
     * @return RSSI in dBm
     */
    int getRSSI() const;
    
    /**
     * @brief Check if connected to a network
     * @return true if in station mode and connected
     */
    bool isConnected() const;
    
    /**
     * @brief Check if in AP mode
     * @return true if access point is active
     */
    bool isAPMode() const;
    
private:
    State _state = State::DISCONNECTED;
    DNSServer _dnsServer;
    bool _dnsRunning = false;
    uint32_t _connectStartTime = 0;
    uint8_t _connectAttempts = 0;
    String _currentSSID;
    String _currentPassword;
    
    void _handleConnection();
    void _startDNS();
    void _stopDNS();
};

// Global instance
extern WiFiManager wifiManager;

#endif // WIFI_MANAGER_H
