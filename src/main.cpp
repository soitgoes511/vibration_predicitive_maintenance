/**
 * ESP32 Vibration Monitoring System
 * 
 * Main application for predictive maintenance vibration data collection.
 * 
 * Features:
 * - ADXL313 3-axis accelerometer sampling at 3200 Hz
 * - PLC trigger input for synchronized measurements
 * - Butterworth low-pass filtering
 * - FFT for frequency domain analysis
 * - InfluxDB 2.x data upload
 * - Web-based configuration with captive portal
 * 
 * Hardware:
 * - ESP32-WROOM-32
 * - ADXL313 accelerometer (SPI)
 * - PLC trigger input (GPIO)
 * 
 * Author: Migrated from Raspberry Pi Python implementation
 * Date: 2024
 */

#include <Arduino.h>
#include "config.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "adxl313.h"
#include "dsp.h"
#include "influxdb_client.h"
#include <sys/time.h>

// ============================================================================
// Global State
// ============================================================================
volatile bool triggerPending = false;
volatile uint32_t lastTriggerTime = 0;
portMUX_TYPE triggerMux = portMUX_INITIALIZER_UNLOCKED;

// Sampling buffers (allocated dynamically)
float* bufferX = nullptr;
float* bufferY = nullptr;
float* bufferZ = nullptr;
float* freqBins = nullptr;
float* fftX = nullptr;
float* fftY = nullptr;
float* fftZ = nullptr;

size_t currentSampleCount = 0;

// Run ID for InfluxDB
char runId[64];
uint32_t runSequence = 0;
uint64_t lastUploadTimestampNs = 0;

static constexpr const char* FW_VERSION = "1.1.0";

static bool getCurrentEpochTimestampNs(uint64_t& timestampNs) {
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) != 0) {
        return false;
    }

    // Reject invalid/unsynced RTC values.
    if (tv.tv_sec < 1700000000) {
        return false;
    }

    timestampNs = (uint64_t)tv.tv_sec * 1000000000ULL + (uint64_t)tv.tv_usec * 1000ULL;
    return true;
}

static float getRangeGFromSensitivity(uint8_t sensitivity) {
    switch (sensitivity) {
        case ADXL313_RANGE_0_5G: return 0.5f;
        case ADXL313_RANGE_1G:   return 1.0f;
        case ADXL313_RANGE_2G:   return 2.0f;
        case ADXL313_RANGE_4G:   return 4.0f;
        default:                 return 2.0f;
    }
}

// ============================================================================
// ISR: PLC Trigger
// ============================================================================
void IRAM_ATTR plcTriggerISR() {
    portENTER_CRITICAL_ISR(&triggerMux);
    
    // Debounce check
    uint32_t now = millis();
    if (now - lastTriggerTime > 100) {  // 100ms debounce
        triggerPending = true;
        lastTriggerTime = now;
    }
    
    portEXIT_CRITICAL_ISR(&triggerMux);
}

// ============================================================================
// Buffer Management
// ============================================================================
bool allocateBuffers(size_t sampleCount) {
    // Free existing buffers
    if (bufferX) { free(bufferX); bufferX = nullptr; }
    if (bufferY) { free(bufferY); bufferY = nullptr; }
    if (bufferZ) { free(bufferZ); bufferZ = nullptr; }
    if (freqBins) { free(freqBins); freqBins = nullptr; }
    if (fftX) { free(fftX); fftX = nullptr; }
    if (fftY) { free(fftY); fftY = nullptr; }
    if (fftZ) { free(fftZ); fftZ = nullptr; }
    
    // Calculate FFT size (power of 2)
    size_t fftSize = DSP::nextPowerOf2(sampleCount);
    size_t numBins = fftSize / 2 + 1;
    
    // Allocate time-domain buffers
    bufferX = (float*)malloc(sampleCount * sizeof(float));
    bufferY = (float*)malloc(sampleCount * sizeof(float));
    bufferZ = (float*)malloc(sampleCount * sizeof(float));
    
    // Allocate frequency-domain buffers
    freqBins = (float*)malloc(numBins * sizeof(float));
    fftX = (float*)malloc(numBins * sizeof(float));
    fftY = (float*)malloc(numBins * sizeof(float));
    fftZ = (float*)malloc(numBins * sizeof(float));
    
    if (!bufferX || !bufferY || !bufferZ || !freqBins || !fftX || !fftY || !fftZ) {
        Serial.println("[Main] Buffer allocation failed!");
        return false;
    }
    
    currentSampleCount = sampleCount;
    Serial.printf("[Main] Buffers allocated: %d samples, %d freq bins, heap free: %d\n",
                  sampleCount, numBins, ESP.getFreeHeap());
    
    return true;
}

// ============================================================================
// Sampling
// ============================================================================
void performSampling() {
    DeviceConfig& cfg = configManager.getConfig();
    
    Serial.println("\n========================================");
    Serial.println("[Main] Trigger received - starting measurement");
    Serial.println("========================================");
    
    unsigned long startTime = micros();
    
    // Sample interval in microseconds
    uint32_t sampleIntervalUs = 1000000 / cfg.sample_rate_hz;
    uint32_t nextSampleTime = micros();
    
    // Disable WiFi interrupts for consistent timing
    WiFi.setSleep(true);
    
    // Sampling loop
    for (size_t i = 0; i < currentSampleCount; i++) {
        // Wait for next sample time
        while (micros() < nextSampleTime) {
            // Busy wait for timing accuracy
        }
        nextSampleTime += sampleIntervalUs;
        
        // Read accelerometer
        float x, y, z;
        if (adxl313.readAccel(x, y, z)) {
            bufferX[i] = x;
            bufferY[i] = y;
            bufferZ[i] = z;
        } else {
            bufferX[i] = 0;
            bufferY[i] = 0;
            bufferZ[i] = 0;
        }
    }
    
    // Re-enable WiFi
    WiFi.setSleep(false);
    
    unsigned long endTime = micros();
    float actualDuration = (endTime - startTime) / 1000000.0f;
    float actualRate = currentSampleCount / actualDuration;
    
    Serial.printf("[Main] Sampling complete: %d samples in %.3f seconds\n", 
                  currentSampleCount, actualDuration);
    Serial.printf("[Main] Actual sampling rate: %.1f Hz\n", actualRate);
}

// ============================================================================
// Signal Processing
// ============================================================================
void processData() {
    DeviceConfig& cfg = configManager.getConfig();
    
    Serial.println("[Main] Processing data...");
    unsigned long startTime = millis();
    
    // Design and apply Butterworth filter
    dsp.designButterworth(cfg.filter_cutoff_hz, cfg.sample_rate_hz, 4);
    
    dsp.applyFiltFilt(bufferX, currentSampleCount);
    dsp.applyFiltFilt(bufferY, currentSampleCount);
    dsp.applyFiltFilt(bufferZ, currentSampleCount);
    
    Serial.printf("[Main] Filtering complete, heap: %d\n", ESP.getFreeHeap());
    
    // Compute FFT for each axis
    // Note: We need copies because FFT modifies input
    float* tempBuffer = (float*)malloc(currentSampleCount * sizeof(float));
    if (!tempBuffer) {
        Serial.println("[Main] FFT temp buffer allocation failed!");
        return;
    }
    
    // FFT for X axis
    memcpy(tempBuffer, bufferX, currentSampleCount * sizeof(float));
    size_t numBins = dsp.computeFFT(tempBuffer, fftX, currentSampleCount, cfg.sample_rate_hz);
    
    // FFT for Y axis
    memcpy(tempBuffer, bufferY, currentSampleCount * sizeof(float));
    dsp.computeFFT(tempBuffer, fftY, currentSampleCount, cfg.sample_rate_hz);
    
    // FFT for Z axis
    memcpy(tempBuffer, bufferZ, currentSampleCount * sizeof(float));
    dsp.computeFFT(tempBuffer, fftZ, currentSampleCount, cfg.sample_rate_hz);
    
    free(tempBuffer);
    
    size_t fftSize = DSP::nextPowerOf2(currentSampleCount);

    // Calculate frequency values for each bin
    for (size_t i = 0; i < numBins; i++) {
        // Use actual FFT length (after zero-padding), not raw sample count.
        freqBins[i] = DSP::binToFrequency(i, fftSize, cfg.sample_rate_hz);
    }
    
    unsigned long processingTime = millis() - startTime;
    Serial.printf("[Main] Processing complete in %d ms, %d frequency bins\n", 
                  processingTime, numBins);
}

// ============================================================================
// Data Upload
// ============================================================================
void uploadData() {
    DeviceConfig& cfg = configManager.getConfig();
    
    if (!configManager.isInfluxConfigured()) {
        Serial.println("[Main] InfluxDB not configured, skipping upload");
        return;
    }
    
    if (!wifiManager.isConnected()) {
        Serial.println("[Main] WiFi not connected, skipping upload");
        return;
    }
    
    Serial.println("[Main] Uploading data to InfluxDB...");
    unsigned long startTime = millis();

    if (!wifiManager.hasValidTime()) {
        Serial.println("[Main] Time not synchronized, attempting SNTP sync...");
        if (!wifiManager.syncTime()) {
            Serial.println("[Main] SNTP time unavailable, skipping upload");
            webServer.updateStatus(lastTriggerTime, currentSampleCount, false);
            return;
        }
    }

    uint64_t baseTimestampNs = 0;
    if (!getCurrentEpochTimestampNs(baseTimestampNs)) {
        Serial.println("[Main] Failed to read epoch timestamp, skipping upload");
        webServer.updateStatus(lastTriggerTime, currentSampleCount, false);
        return;
    }

    // Guarantee strictly increasing run timestamps to avoid collisions.
    if (baseTimestampNs <= lastUploadTimestampNs) {
        baseTimestampNs = lastUploadTimestampNs + 1;
    }
    lastUploadTimestampNs = baseTimestampNs;

    String deviceId = configManager.getDeviceId();
    runSequence++;
    snprintf(runId, sizeof(runId), "%s-%llu-%lu",
             deviceId.c_str(),
             (unsigned long long)(baseTimestampNs / 1000000000ULL),
             (unsigned long)runSequence);
    Serial.printf("[Main] Run ID: %s\n", runId);
    
    // Calculate number of frequency bins
    size_t fftSize = DSP::nextPowerOf2(currentSampleCount);
    size_t numBins = fftSize / 2 + 1;
    
    bool success = true;

    // Upload run metadata first for traceability
    success &= influxClient.writeRunMetadata(
        cfg.operation_id, deviceId.c_str(), runId,
        cfg.sample_rate_hz, currentSampleCount, fftSize,
        cfg.filter_cutoff_hz, getRangeGFromSensitivity(cfg.sensitivity),
        cfg.send_time_domain, FW_VERSION,
        baseTimestampNs
    );
    
    // Upload frequency domain data
    success &= influxClient.writeFrequencyData(
        cfg.operation_id, deviceId.c_str(), runId,
        freqBins, fftX, fftY, fftZ,
        numBins, baseTimestampNs
    );
    
    // Optionally upload time domain data
    if (cfg.send_time_domain) {
        success &= influxClient.writeTimeData(
            cfg.operation_id, deviceId.c_str(), runId,
            bufferX, bufferY, bufferZ,
            currentSampleCount, baseTimestampNs,
            cfg.sample_rate_hz
        );
    }
    
    unsigned long uploadTime = millis() - startTime;
    
    if (success) {
        Serial.printf("[Main] Upload complete in %d ms\n", uploadTime);
    } else {
        Serial.println("[Main] Upload failed!");
    }
    
    // Update web server status
    webServer.updateStatus(lastTriggerTime, currentSampleCount, success);
}

// ============================================================================
// Manual Trigger Callback
// ============================================================================
void manualTriggerCallback() {
    portENTER_CRITICAL(&triggerMux);
    triggerPending = true;
    lastTriggerTime = millis();
    portEXIT_CRITICAL(&triggerMux);
    
    Serial.println("[Main] Manual trigger requested");
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n\n");
    Serial.println("==============================================");
    Serial.println("  ESP32 Vibration Monitoring System");
    Serial.println("  Predictive Maintenance Data Collector");
    Serial.println("==============================================\n");
    
    // Initialize configuration manager
    Serial.println("[Main] Loading configuration...");
    if (!configManager.begin()) {
        Serial.println("[Main] Using default configuration");
    }
    
    DeviceConfig& cfg = configManager.getConfig();
    
    // Initialize DSP
    Serial.println("[Main] Initializing DSP...");
    if (!dsp.begin()) {
        Serial.println("[Main] DSP init failed!");
    }
    
    // Initialize ADXL313
    Serial.println("[Main] Initializing ADXL313...");
    if (!adxl313.begin(cfg.spi_cs_pin)) {
        Serial.println("[Main] ADXL313 init failed! Check wiring.");
    } else {
        adxl313.setSensitivity(cfg.sensitivity);
    }
    
    // Allocate sampling buffers
    Serial.println("[Main] Allocating buffers...");
    if (!allocateBuffers(cfg.sample_count)) {
        Serial.println("[Main] Buffer allocation failed!");
    }
    
    // Initialize InfluxDB client
    Serial.println("[Main] Configuring InfluxDB client...");
    influxClient.begin(cfg.influx_url, cfg.influx_token, 
                       cfg.influx_org, cfg.influx_bucket);
    
    // Initialize WiFi
    Serial.println("[Main] Initializing WiFi...");
    wifiManager.begin();
    
    // Initialize web server
    Serial.println("[Main] Starting web server...");
    webServer.begin();
    webServer.setTriggerCallback(manualTriggerCallback);
    
    // Configure PLC trigger input
    Serial.printf("[Main] Configuring PLC trigger on GPIO %d...\n", cfg.plc_trigger_pin);
    pinMode(cfg.plc_trigger_pin, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(cfg.plc_trigger_pin), plcTriggerISR, RISING);
    
    Serial.println("\n[Main] System ready!");
    Serial.printf("[Main] Device ID: %s\n", configManager.getDeviceId().c_str());
    Serial.printf("[Main] Operation: %s\n", cfg.operation_id);
    Serial.printf("[Main] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("\nWaiting for trigger...\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    // Process WiFi events
    wifiManager.loop();
    
    // Check for trigger
    bool shouldProcess = false;
    portENTER_CRITICAL(&triggerMux);
    if (triggerPending) {
        triggerPending = false;
        shouldProcess = true;
    }
    portEXIT_CRITICAL(&triggerMux);
    
    if (shouldProcess) {
        // Perform complete measurement cycle
        performSampling();
        processData();
        uploadData();
        
        Serial.println("\n[Main] Measurement cycle complete, waiting for next trigger...\n");
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}
