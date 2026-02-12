#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// Default Pin Mappings (ESP32-WROOM-32 VSPI)
// ============================================================================
#define DEFAULT_SPI_MOSI    23
#define DEFAULT_SPI_MISO    19
#define DEFAULT_SPI_CLK     18
#define DEFAULT_SPI_CS      5
#define DEFAULT_PLC_TRIGGER 4

// ============================================================================
// ADXL313 Registers
// ============================================================================
#define ADXL313_DEVID       0x00
#define ADXL313_POWER_CTL   0x2D
#define ADXL313_DATA_FORMAT 0x31
#define ADXL313_BW_RATE     0x2C
#define ADXL313_DATAX0      0x32
#define ADXL313_READ_BIT    0x80
#define ADXL313_MULTI_BIT   0x40

// Sensitivity ranges
#define ADXL313_RANGE_0_5G  0  // ±0.5g
#define ADXL313_RANGE_1G    1  // ±1g
#define ADXL313_RANGE_2G    2  // ±2g
#define ADXL313_RANGE_4G    3  // ±4g

// Scale factors (g per LSB) for each range
constexpr float ADXL313_SCALE[] = {
    0.5f / 512.0f,   // ±0.5g: 1024 counts full scale
    1.0f / 512.0f,   // ±1g
    2.0f / 512.0f,   // ±2g
    4.0f / 512.0f    // ±4g
};

// ============================================================================
// Sampling Configuration
// ============================================================================
#define DEFAULT_SAMPLE_COUNT     4096    // Power-of-2 window for FFT consistency
#define DEFAULT_SAMPLE_RATE_HZ   3200    // Maximum ADXL313 rate
#define DEFAULT_FILTER_CUTOFF_HZ 1600    // Nyquist / 2 for anti-aliasing

// Maximum values (for buffer allocation)
#define MAX_SAMPLE_COUNT         8000
#define MAX_OPERATION_ID_LEN     32

// ============================================================================
// WiFi Configuration
// ============================================================================
#define WIFI_AP_PREFIX           "VibSensor_"
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_RETRY_COUNT         3

// ============================================================================
// InfluxDB Configuration
// ============================================================================
#define INFLUX_WRITE_BATCH_SIZE  500     // Points per HTTP request
#define INFLUX_RETRY_COUNT       3
#define INFLUX_TIMEOUT_MS        10000

// ============================================================================
// Device Configuration Structure
// ============================================================================
struct DeviceConfig {
    // Magic number for validation
    uint32_t magic;
    
    // WiFi credentials
    char wifi_ssid[32];
    char wifi_password[64];
    
    // InfluxDB connection
    char influx_url[128];       // e.g., "http://192.168.1.100:8086"
    char influx_token[128];
    char influx_org[32];
    char influx_bucket[32];
    
    // Operation identification
    char operation_id[MAX_OPERATION_ID_LEN];  // e.g., "L9OP600"
    
    // Hardware pin configuration
    uint8_t plc_trigger_pin;
    uint8_t spi_cs_pin;
    
    // ADXL313 settings
    uint8_t sensitivity;        // 0=±0.5g, 1=±1g, 2=±2g, 3=±4g
    
    // Sampling parameters
    uint16_t sample_count;
    uint16_t sample_rate_hz;
    uint16_t filter_cutoff_hz;
    
    // Runtime flags
    bool send_time_domain;      // Whether to send time-domain data to InfluxDB
};

// Magic number for config validation
#define CONFIG_MAGIC 0xADC31300

// Default configuration
inline DeviceConfig getDefaultConfig() {
    DeviceConfig cfg = {};
    cfg.magic = CONFIG_MAGIC;
    
    // WiFi - empty means AP mode
    cfg.wifi_ssid[0] = '\0';
    cfg.wifi_password[0] = '\0';
    
    // InfluxDB defaults (must be configured)
    strcpy(cfg.influx_url, "http://192.168.1.100:8086");
    cfg.influx_token[0] = '\0';
    strcpy(cfg.influx_org, "expertise");
    strcpy(cfg.influx_bucket, "expertise");
    
    // Operation ID
    strcpy(cfg.operation_id, "L9OP600");
    
    // Hardware
    cfg.plc_trigger_pin = DEFAULT_PLC_TRIGGER;
    cfg.spi_cs_pin = DEFAULT_SPI_CS;
    
    // Sensor
    cfg.sensitivity = ADXL313_RANGE_2G;  // ±2g default
    
    // Sampling
    cfg.sample_count = DEFAULT_SAMPLE_COUNT;
    cfg.sample_rate_hz = DEFAULT_SAMPLE_RATE_HZ;
    cfg.filter_cutoff_hz = DEFAULT_FILTER_CUTOFF_HZ;
    
    // Time-domain data disabled by default to save memory/bandwidth
    cfg.send_time_domain = false;
    
    return cfg;
}

#endif // CONFIG_H
