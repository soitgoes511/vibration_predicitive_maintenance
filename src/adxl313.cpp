#include "adxl313.h"

// Global instance
ADXL313 adxl313;

bool ADXL313::begin(uint8_t csPin, uint32_t spiSpeed) {
    _csPin = csPin;
    _spiSettings = SPISettings(spiSpeed, MSBFIRST, SPI_MODE3);
    
    // Initialize CS pin
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    
    // Initialize SPI
    SPI.begin();
    delay(10);
    
    // Verify device ID
    uint8_t devId = readDeviceId();
    Serial.printf("[ADXL313] Device ID: 0x%02X\n", devId);
    
    if (devId != 0xAD) {
        Serial.println("[ADXL313] ERROR: Invalid device ID!");
        return false;
    }
    
    // Configure sensor (matching Python implementation)
    // POWER_CTL: 0x48 = I2C disable (bit 6) + Measure mode (bit 3)
    _writeReg(ADXL313_POWER_CTL, 0x48);
    delay(5);
    
    // Set default sensitivity (±2g)
    setSensitivity(ADXL313_RANGE_2G);
    
    // BW_RATE: 0x0F = 3200 Hz output data rate
    setDataRate(0x0F);
    
    Serial.println("[ADXL313] Initialized successfully");
    return true;
}

void ADXL313::setSensitivity(uint8_t range) {
    if (range > ADXL313_RANGE_4G) {
        range = ADXL313_RANGE_4G;
    }
    
    _sensitivity = range;
    _scale = ADXL313_SCALE[range];
    
    // DATA_FORMAT register:
    // Bit 3 (FULL_RES): 0 = 10-bit mode
    // Bits 1:0 (Range): 00=±0.5g, 01=±1g, 10=±2g, 11=±4g
    _writeReg(ADXL313_DATA_FORMAT, range);
    
    Serial.printf("[ADXL313] Sensitivity set to range %d (scale: %.6f g/LSB)\n", 
                  range, _scale);
}

uint8_t ADXL313::getSensitivity() const {
    return _sensitivity;
}

void ADXL313::setDataRate(uint8_t rate) {
    _writeReg(ADXL313_BW_RATE, rate);
    Serial.printf("[ADXL313] Data rate register set to 0x%02X\n", rate);
}

bool ADXL313::readRaw(int16_t& x, int16_t& y, int16_t& z) {
    uint8_t buffer[6];
    
    // Read 6 bytes starting from DATAX0 (burst read)
    _readBurst(ADXL313_DATAX0, buffer, 6);
    
    // Combine bytes (little endian)
    x = (int16_t)((buffer[1] << 8) | buffer[0]);
    y = (int16_t)((buffer[3] << 8) | buffer[2]);
    z = (int16_t)((buffer[5] << 8) | buffer[4]);
    
    return true;
}

bool ADXL313::readAccel(float& x, float& y, float& z) {
    int16_t rawX, rawY, rawZ;
    
    if (!readRaw(rawX, rawY, rawZ)) {
        return false;
    }
    
    // Convert to g units
    x = rawX * _scale;
    y = rawY * _scale;
    z = rawZ * _scale;
    
    return true;
}

uint8_t ADXL313::readDeviceId() {
    return _readReg(ADXL313_DEVID);
}

bool ADXL313::isConnected() {
    return readDeviceId() == 0xAD;
}

void ADXL313::_writeReg(uint8_t reg, uint8_t value) {
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    
    SPI.transfer(reg & 0x3F);  // Write: bit 7 = 0
    SPI.transfer(value);
    
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

uint8_t ADXL313::_readReg(uint8_t reg) {
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    
    SPI.transfer(reg | ADXL313_READ_BIT);  // Read: bit 7 = 1
    uint8_t value = SPI.transfer(0x00);
    
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
    
    return value;
}

void ADXL313::_readBurst(uint8_t reg, uint8_t* buffer, uint8_t len) {
    SPI.beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    
    // Read + Multi-byte: bits 7 and 6 set
    SPI.transfer(reg | ADXL313_READ_BIT | ADXL313_MULTI_BIT);
    
    for (uint8_t i = 0; i < len; i++) {
        buffer[i] = SPI.transfer(0x00);
    }
    
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}
