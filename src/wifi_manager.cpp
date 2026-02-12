#include "wifi_manager.h"
#include "config_manager.h"
#include <time.h>

// DNS server port
static const uint16_t DNS_PORT = 53;
static const time_t MIN_VALID_EPOCH = 1700000000;   // Approx. 2023-11-14 UTC
static const uint32_t TIME_SYNC_RETRY_MS = 30000;

static bool isSystemTimeValid() {
    return time(nullptr) >= MIN_VALID_EPOCH;
}

// Global instance
WiFiManager wifiManager;

void WiFiManager::begin() {
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    Serial.println("[WiFi] Initializing...");
    
    _timeSynced = isSystemTimeValid();

    // Check if WiFi is configured
    if (configManager.isWifiConfigured()) {
        DeviceConfig& cfg = configManager.getConfig();
        connectStation(cfg.wifi_ssid, cfg.wifi_password);
    } else {
        Serial.println("[WiFi] No WiFi configured, starting AP mode");
        startAP(configManager.getAPName());
    }
}

void WiFiManager::loop() {
    _handleConnection();
    
    // Process DNS requests in AP mode
    if (_dnsRunning) {
        _dnsServer.processNextRequest();
    }
}

void WiFiManager::startAP(const String& apName) {
    Serial.printf("[WiFi] Starting AP: %s\n", apName.c_str());
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str());
    
    // Wait for AP to start
    delay(100);
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] AP IP: %s\n", ip.toString().c_str());
    
    _startDNS();
    _state = State::AP_MODE;
}

bool WiFiManager::connectStation(const char* ssid, const char* password) {
    if (strlen(ssid) == 0) {
        return false;
    }
    
    Serial.printf("[WiFi] Connecting to: %s\n", ssid);
    
    _stopDNS();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    _currentSSID = ssid;
    _currentPassword = password;
    _connectStartTime = millis();
    _connectAttempts = 0;
    _state = State::CONNECTING;
    
    return true;
}

void WiFiManager::reconnect() {
    if (_currentSSID.length() > 0) {
        connectStation(_currentSSID.c_str(), _currentPassword.c_str());
    }
}

void WiFiManager::_handleConnection() {
    switch (_state) {
        case State::CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
                _state = State::CONNECTED_STATION;
                _syncTimeIfNeeded();
            } else if (millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
                _connectAttempts++;
                Serial.printf("[WiFi] Connection timeout (attempt %d/%d)\n", 
                              _connectAttempts, WIFI_RETRY_COUNT);
                
                if (_connectAttempts >= WIFI_RETRY_COUNT) {
                    Serial.println("[WiFi] Max retries reached, falling back to AP mode");
                    startAP(configManager.getAPName());
                } else {
                    // Retry connection
                    WiFi.disconnect();
                    delay(100);
                    WiFi.begin(_currentSSID.c_str(), _currentPassword.c_str());
                    _connectStartTime = millis();
                }
            }
            break;
            
        case State::CONNECTED_STATION:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost, reconnecting...");
                _state = State::CONNECTING;
                _connectStartTime = millis();
                _connectAttempts = 0;
                WiFi.reconnect();
            } else if (!hasValidTime() && (millis() - _lastTimeSyncAttemptMs) > TIME_SYNC_RETRY_MS) {
                _syncTimeIfNeeded();
            }
            break;
            
        case State::AP_MODE:
        case State::DISCONNECTED:
            // Nothing to do
            break;
    }
}

void WiFiManager::_startDNS() {
    if (!_dnsRunning) {
        // Redirect all DNS queries to our IP (captive portal)
        _dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
        _dnsRunning = true;
        Serial.println("[WiFi] DNS server started (captive portal)");
    }
}

void WiFiManager::_stopDNS() {
    if (_dnsRunning) {
        _dnsServer.stop();
        _dnsRunning = false;
        Serial.println("[WiFi] DNS server stopped");
    }
}

WiFiManager::State WiFiManager::getState() const {
    return _state;
}

String WiFiManager::getIP() const {
    if (_state == State::AP_MODE) {
        return WiFi.softAPIP().toString();
    } else if (_state == State::CONNECTED_STATION) {
        return WiFi.localIP().toString();
    }
    return "0.0.0.0";
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

bool WiFiManager::isConnected() const {
    return _state == State::CONNECTED_STATION;
}

bool WiFiManager::isAPMode() const {
    return _state == State::AP_MODE;
}

bool WiFiManager::hasValidTime() const {
    return isSystemTimeValid();
}

bool WiFiManager::syncTime(uint32_t timeoutMs) {
    if (_state != State::CONNECTED_STATION || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    if (hasValidTime()) {
        _timeSynced = true;
        return true;
    }

    if (!_ntpConfigured) {
        configTzTime("UTC0", "pool.ntp.org", "time.google.com", "time.windows.com");
        _ntpConfigured = true;
        Serial.println("[WiFi] SNTP configured");
    }

    _lastTimeSyncAttemptMs = millis();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, timeoutMs)) {
        _timeSynced = true;
        Serial.printf("[WiFi] Time synchronized: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        return true;
    }

    _timeSynced = hasValidTime();
    if (!_timeSynced) {
        Serial.println("[WiFi] SNTP sync not ready yet");
    }
    return _timeSynced;
}

void WiFiManager::_syncTimeIfNeeded() {
    if (_state != State::CONNECTED_STATION) {
        return;
    }
    if (hasValidTime()) {
        _timeSynced = true;
        return;
    }
    syncTime();
}
