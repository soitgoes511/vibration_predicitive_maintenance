#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "config.h"

/**
 * @brief Configuration manager for persistent NVS storage
 * 
 * Handles loading, saving, and validating device configuration
 * stored in ESP32's Non-Volatile Storage (NVS).
 */
class ConfigManager {
public:
    /**
     * @brief Initialize the configuration manager
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Load configuration from NVS
     * @return true if valid configuration was loaded
     */
    bool load();
    
    /**
     * @brief Save current configuration to NVS
     * @return true if save was successful
     */
    bool save();
    
    /**
     * @brief Reset configuration to factory defaults
     */
    void resetToDefaults();
    
    /**
     * @brief Get reference to current configuration
     * @return Reference to DeviceConfig structure
     */
    DeviceConfig& getConfig();
    
    /**
     * @brief Get unique device ID based on MAC address
     * @return String in format "XXXX" (last 4 hex chars of MAC)
     */
    String getDeviceId();
    
    /**
     * @brief Get full unique AP name
     * @return String in format "VibSensor_XXXX"
     */
    String getAPName();
    
    /**
     * @brief Check if WiFi is configured
     * @return true if SSID is set
     */
    bool isWifiConfigured();
    
    /**
     * @brief Check if InfluxDB is configured
     * @return true if URL and token are set
     */
    bool isInfluxConfigured();
    
private:
    DeviceConfig _config;
    bool _initialized = false;
    String _deviceId;
    
    void _generateDeviceId();
};

// Global instance
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
