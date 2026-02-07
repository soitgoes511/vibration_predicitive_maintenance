#include "influxdb_client.h"
#include "config.h"
#include <WiFi.h>

// Global instance
InfluxDBClient influxClient;

void InfluxDBClient::begin(const char* url, const char* token, 
                           const char* org, const char* bucket) {
    setConnection(url, token, org, bucket);
}

void InfluxDBClient::setConnection(const char* url, const char* token,
                                   const char* org, const char* bucket) {
    _url = url;
    _token = token;
    _org = org;
    _bucket = bucket;
    
    Serial.printf("[InfluxDB] Configured: %s, org=%s, bucket=%s\n",
                  _url.c_str(), _org.c_str(), _bucket.c_str());
}

bool InfluxDBClient::isConfigured() const {
    return _url.length() > 0 && _token.length() > 0;
}

String InfluxDBClient::_buildWriteUrl() {
    String url = _url;
    if (!url.endsWith("/")) url += "/";
    url += "api/v2/write?org=";
    url += _org;
    url += "&bucket=";
    url += _bucket;
    url += "&precision=ns";
    return url;
}

bool InfluxDBClient::testConnection() {
    if (!isConfigured()) {
        _lastError = "Client not configured";
        return false;
    }
    
    String healthUrl = _url;
    if (!healthUrl.endsWith("/")) healthUrl += "/";
    healthUrl += "health";
    
    _http.begin(healthUrl);
    _http.setTimeout(5000);
    
    int httpCode = _http.GET();
    String response = _http.getString();
    _http.end();
    
    if (httpCode == 200) {
        Serial.println("[InfluxDB] Health check passed");
        return true;
    }
    
    _lastError = "Health check failed: " + String(httpCode) + " - " + response;
    Serial.printf("[InfluxDB] %s\n", _lastError.c_str());
    return false;
}

bool InfluxDBClient::_sendLineProtocol(const String& lineProtocol) {
    if (!isConfigured()) {
        _lastError = "Client not configured";
        return false;
    }
    
    String writeUrl = _buildWriteUrl();
    
    for (int attempt = 0; attempt < INFLUX_RETRY_COUNT; attempt++) {
        _http.begin(writeUrl);
        _http.setTimeout(INFLUX_TIMEOUT_MS);
        _http.addHeader("Content-Type", "text/plain");
        _http.addHeader("Authorization", "Token " + _token);
        
        int httpCode = _http.POST(lineProtocol);
        String response = _http.getString();
        _http.end();
        
        if (httpCode >= 200 && httpCode < 300) {
            return true;
        }
        
        _lastError = "Write failed: " + String(httpCode) + " - " + response;
        Serial.printf("[InfluxDB] Attempt %d failed: %s\n", attempt + 1, _lastError.c_str());
        
        // Exponential backoff
        delay(100 * (1 << attempt));
    }
    
    Serial.println("[InfluxDB] All retries failed, dropping data");
    return false;
}

bool InfluxDBClient::writePoint(const char* measurement, const char* tags,
                                const char* fields, uint64_t timestampNs) {
    String line = measurement;
    
    if (tags && strlen(tags) > 0) {
        line += ",";
        line += tags;
    }
    
    line += " ";
    line += fields;
    
    if (timestampNs > 0) {
        line += " ";
        line += String((uint32_t)(timestampNs / 1000000000ULL));  // Seconds
        line += String((uint32_t)(timestampNs % 1000000000ULL));  // Nanoseconds
    }
    
    return _sendLineProtocol(line);
}

bool InfluxDBClient::writeFrequencyData(const char* operationId, const char* runId,
                                        const float* frequencies,
                                        const float* xFreq, const float* yFreq, 
                                        const float* zFreq, size_t numBins,
                                        uint64_t baseTimestampNs) {
    Serial.printf("[InfluxDB] Writing %d frequency bins\n", numBins);
    
    // Build line protocol in batches
    String batch;
    batch.reserve(INFLUX_WRITE_BATCH_SIZE * 150);  // Estimate ~150 chars per point
    
    size_t batchCount = 0;
    uint64_t timestampIncrement = 1000000;  // 1ms between "points" for visualization
    
    for (size_t i = 0; i < numBins; i++) {
        // Skip DC component (i=0) as in Python code
        if (i == 0) continue;
        
        // measurement,tags fields timestamp
        char point[256];
        snprintf(point, sizeof(point),
                 "accelfreq,operation=%s,run_id=%s frequencies=%.6f,x_freq=%.6f,y_freq=%.6f,z_freq=%.6f %llu\n",
                 operationId, runId,
                 frequencies[i], xFreq[i], yFreq[i], zFreq[i],
                 baseTimestampNs + (i * timestampIncrement));
        
        batch += point;
        batchCount++;
        
        // Send batch when full
        if (batchCount >= INFLUX_WRITE_BATCH_SIZE) {
            if (!_sendLineProtocol(batch)) {
                return false;
            }
            batch = "";
            batch.reserve(INFLUX_WRITE_BATCH_SIZE * 150);
            batchCount = 0;
        }
    }
    
    // Send remaining points
    if (batchCount > 0) {
        if (!_sendLineProtocol(batch)) {
            return false;
        }
    }
    
    Serial.println("[InfluxDB] Frequency data written successfully");
    return true;
}

bool InfluxDBClient::writeTimeData(const char* operationId, const char* runId,
                                   const float* x, const float* y, const float* z,
                                   size_t numSamples, uint64_t baseTimestampNs,
                                   float sampleRateHz) {
    Serial.printf("[InfluxDB] Writing %d time samples\n", numSamples);
    
    // Calculate time increment between samples in nanoseconds
    uint64_t sampleIntervalNs = (uint64_t)(1000000000.0 / sampleRateHz);
    
    String batch;
    batch.reserve(INFLUX_WRITE_BATCH_SIZE * 100);
    
    size_t batchCount = 0;
    
    for (size_t i = 0; i < numSamples; i++) {
        char point[192];
        snprintf(point, sizeof(point),
                 "acceltime,operation=%s,run_id=%s x=%.6f,y=%.6f,z=%.6f %llu\n",
                 operationId, runId,
                 x[i], y[i], z[i],
                 baseTimestampNs + (i * sampleIntervalNs));
        
        batch += point;
        batchCount++;
        
        if (batchCount >= INFLUX_WRITE_BATCH_SIZE) {
            if (!_sendLineProtocol(batch)) {
                return false;
            }
            batch = "";
            batch.reserve(INFLUX_WRITE_BATCH_SIZE * 100);
            batchCount = 0;
        }
    }
    
    if (batchCount > 0) {
        if (!_sendLineProtocol(batch)) {
            return false;
        }
    }
    
    Serial.println("[InfluxDB] Time data written successfully");
    return true;
}

String InfluxDBClient::getLastError() const {
    return _lastError;
}
