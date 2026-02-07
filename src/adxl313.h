#ifndef ADXL313_H
#define ADXL313_H

#include <Arduino.h>
#include <SPI.h>
#include "config.h"

/**
 * @brief ADXL313 3-axis accelerometer driver via SPI
 * 
 * Ported from Python implementation with high-speed sampling support
 * using hardware timers for consistent 3200 Hz data acquisition.
 */
class ADXL313 {
public:
    /**
     * @brief Initialize the ADXL313 sensor
     * @param csPin Chip select GPIO pin
     * @param spiSpeed SPI clock speed in Hz
     * @return true if initialization successful
     */
    bool begin(uint8_t csPin = DEFAULT_SPI_CS, uint32_t spiSpeed = 5000000);
    
    /**
     * @brief Set measurement sensitivity range
     * @param range ADXL313_RANGE_0_5G, _1G, _2G, or _4G
     */
    void setSensitivity(uint8_t range);
    
    /**
     * @brief Get current sensitivity range
     * @return Current range setting (0-3)
     */
    uint8_t getSensitivity() const;
    
    /**
     * @brief Read raw acceleration data
     * @param x Output: X-axis raw value
     * @param y Output: Y-axis raw value
     * @param z Output: Z-axis raw value
     * @return true if read successful
     */
    bool readRaw(int16_t& x, int16_t& y, int16_t& z);
    
    /**
     * @brief Read acceleration in g units
     * @param x Output: X-axis in g
     * @param y Output: Y-axis in g
     * @param z Output: Z-axis in g
     * @return true if read successful
     */
    bool readAccel(float& x, float& y, float& z);
    
    /**
     * @brief Read device ID register
     * @return Device ID (should be 0xAD for ADXL31x)
     */
    uint8_t readDeviceId();
    
    /**
     * @brief Check if sensor is responding
     * @return true if device ID is valid
     */
    bool isConnected();
    
    /**
     * @brief Set output data rate
     * @param rate BW_RATE register value (0x0F = 3200 Hz)
     */
    void setDataRate(uint8_t rate);
    
private:
    uint8_t _csPin;
    uint8_t _sensitivity;
    float _scale;
    SPISettings _spiSettings;
    
    /**
     * @brief Write to a register
     * @param reg Register address
     * @param value Value to write
     */
    void _writeReg(uint8_t reg, uint8_t value);
    
    /**
     * @brief Read from a register
     * @param reg Register address
     * @return Register value
     */
    uint8_t _readReg(uint8_t reg);
    
    /**
     * @brief Read multiple bytes starting from a register
     * @param reg Starting register address
     * @param buffer Output buffer
     * @param len Number of bytes to read
     */
    void _readBurst(uint8_t reg, uint8_t* buffer, uint8_t len);
};

// Global instance
extern ADXL313 adxl313;

#endif // ADXL313_H
