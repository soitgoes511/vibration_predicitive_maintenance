#include "config_manager.h"
#include <Preferences.h>
#include <WiFi.h>

// NVS namespace and key
static const char* NVS_NAMESPACE = "vibsensor";
static const char* NVS_KEY = "config";

// Global instance
ConfigManager configManager;

bool ConfigManager::begin() {
    _generateDeviceId();
    _initialized = true;
    return load();
}

bool ConfigManager::load() {
    Preferences prefs;
    
    if (!prefs.begin(NVS_NAMESPACE, true)) {  // Read-only
        Serial.println("[Config] Failed to open NVS namespace");
        resetToDefaults();
        return false;
    }
    
    size_t len = prefs.getBytes(NVS_KEY, &_config, sizeof(DeviceConfig));
    prefs.end();
    
    // Validate magic number
    if (len != sizeof(DeviceConfig) || _config.magic != CONFIG_MAGIC) {
        Serial.println("[Config] Invalid or missing config, loading defaults");
        resetToDefaults();
        return false;
    }
    
    Serial.println("[Config] Loaded configuration from NVS");
    Serial.printf("[Config] Operation ID: %s\n", _config.operation_id);
    Serial.printf("[Config] Sensitivity: %d\n", _config.sensitivity);
    Serial.printf("[Config] Sample count: %d\n", _config.sample_count);
    
    return true;
}

bool ConfigManager::save() {
    Preferences prefs;
    
    if (!prefs.begin(NVS_NAMESPACE, false)) {  // Read-write
        Serial.println("[Config] Failed to open NVS for writing");
        return false;
    }
    
    // Ensure magic is set
    _config.magic = CONFIG_MAGIC;
    
    size_t written = prefs.putBytes(NVS_KEY, &_config, sizeof(DeviceConfig));
    prefs.end();
    
    if (written != sizeof(DeviceConfig)) {
        Serial.println("[Config] Failed to write config to NVS");
        return false;
    }
    
    Serial.println("[Config] Configuration saved to NVS");
    return true;
}

void ConfigManager::resetToDefaults() {
    _config = getDefaultConfig();
    Serial.println("[Config] Reset to default configuration");
}

DeviceConfig& ConfigManager::getConfig() {
    return _config;
}

void ConfigManager::_generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    // Use last 2 bytes of MAC for unique ID
    char id[5];
    snprintf(id, sizeof(id), "%02X%02X", mac[4], mac[5]);
    _deviceId = String(id);
    
    Serial.printf("[Config] Device ID: %s\n", _deviceId.c_str());
}

String ConfigManager::getDeviceId() {
    return _deviceId;
}

String ConfigManager::getAPName() {
    return String(WIFI_AP_PREFIX) + _deviceId;
}

bool ConfigManager::isWifiConfigured() {
    return strlen(_config.wifi_ssid) > 0;
}

bool ConfigManager::isInfluxConfigured() {
    return strlen(_config.influx_url) > 0 && strlen(_config.influx_token) > 0;
}
