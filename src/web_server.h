#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

/**
 * @brief Async web server for device configuration
 * 
 * Provides REST API endpoints and serves the configuration web UI.
 * Supports captive portal detection for automatic redirect.
 */
class WebServer {
public:
    /**
     * @brief Start the web server
     * @param port HTTP port (default 80)
     */
    void begin(uint16_t port = 80);
    
    /**
     * @brief Stop the web server
     */
    void stop();
    
    /**
     * @brief Set manual trigger callback
     * @param callback Function to call when manual trigger is requested
     */
    void setTriggerCallback(void (*callback)());
    
    /**
     * @brief Update system status for API responses
     * @param lastTriggerTime Timestamp of last trigger
     * @param sampleCount Number of samples in last acquisition
     * @param influxOk Whether last InfluxDB write succeeded
     */
    void updateStatus(unsigned long lastTriggerTime, size_t sampleCount, bool influxOk);
    
private:
    AsyncWebServer* _server = nullptr;
    void (*_triggerCallback)() = nullptr;
    
    // Status tracking
    unsigned long _lastTriggerTime = 0;
    size_t _lastSampleCount = 0;
    bool _lastInfluxOk = true;
    uint32_t _triggerCount = 0;
    
    // Route handlers
    void _setupRoutes();
    void _handleRoot(AsyncWebServerRequest* request);
    void _handleGetConfig(AsyncWebServerRequest* request);
    void _handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len);
    void _handleGetStatus(AsyncWebServerRequest* request);
    void _handleReset(AsyncWebServerRequest* request);
    void _handleTrigger(AsyncWebServerRequest* request);
    void _handleCaptivePortal(AsyncWebServerRequest* request);
    
    // Helpers
    String _generateConfigJson();
    bool _parseConfigJson(const String& json);
};

// Global instance
extern WebServer webServer;

#endif // WEB_SERVER_H
