#ifndef INFLUXDB_CLIENT_H
#define INFLUXDB_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>

/**
 * @brief InfluxDB 2.x HTTP client for line protocol writes
 * 
 * Handles batched writes to InfluxDB with retry logic and
 * exponential backoff, similar to the Python implementation.
 */
class InfluxDBClient {
public:
    /**
     * @brief Initialize the InfluxDB client
     * @param url InfluxDB server URL (e.g., "http://192.168.1.100:8086")
     * @param token Authentication token
     * @param org Organization name
     * @param bucket Bucket name
     */
    void begin(const char* url, const char* token, const char* org, const char* bucket);
    
    /**
     * @brief Update connection parameters
     */
    void setConnection(const char* url, const char* token, const char* org, const char* bucket);
    
    /**
     * @brief Test connection to InfluxDB server
     * @return true if health check passes
     */
    bool testConnection();
    
    /**
     * @brief Write a single point in line protocol format
     * @param measurement Measurement name
     * @param tags Tag set (e.g., "sensor=adxl313,location=test")
     * @param fields Field set (e.g., "x=1.23,y=4.56,z=7.89")
     * @param timestampNs Timestamp in nanoseconds (0 = server time)
     * @return true if write successful
     */
    bool writePoint(const char* measurement, const char* tags, 
                    const char* fields, uint64_t timestampNs = 0);
    
    /**
     * @brief Write frequency domain data batch
     * @param operationId Operation identifier for tagging
     * @param runId Run timestamp identifier
     * @param frequencies Array of frequency values
     * @param xFreq Array of X-axis frequency magnitudes
     * @param yFreq Array of Y-axis frequency magnitudes
     * @param zFreq Array of Z-axis frequency magnitudes
     * @param numBins Number of frequency bins
     * @param baseTimestampNs Base timestamp in nanoseconds
     * @return true if write successful
     */
    bool writeFrequencyData(const char* operationId, const char* runId,
                            const float* frequencies,
                            const float* xFreq, const float* yFreq, const float* zFreq,
                            size_t numBins, uint64_t baseTimestampNs);
    
    /**
     * @brief Write time domain data batch
     * @param operationId Operation identifier for tagging
     * @param runId Run timestamp identifier
     * @param x Array of X-axis acceleration values
     * @param y Array of Y-axis acceleration values
     * @param z Array of Z-axis acceleration values
     * @param numSamples Number of samples
     * @param baseTimestampNs Base timestamp in nanoseconds
     * @param sampleRateHz Sample rate for timestamp calculation
     * @return true if write successful
     */
    bool writeTimeData(const char* operationId, const char* runId,
                       const float* x, const float* y, const float* z,
                       size_t numSamples, uint64_t baseTimestampNs, 
                       float sampleRateHz);
    
    /**
     * @brief Get last error message
     * @return Error string
     */
    String getLastError() const;
    
    /**
     * @brief Check if client is configured
     * @return true if URL and token are set
     */
    bool isConfigured() const;
    
private:
    String _url;
    String _token;
    String _org;
    String _bucket;
    String _lastError;
    
    HTTPClient _http;
    
    /**
     * @brief Build the write API endpoint URL
     * @return Full URL with query parameters
     */
    String _buildWriteUrl();
    
    /**
     * @brief Send line protocol data with retries
     * @param lineProtocol Line protocol formatted data
     * @return true if write successful
     */
    bool _sendLineProtocol(const String& lineProtocol);
};

// Global instance
extern InfluxDBClient influxClient;

#endif // INFLUXDB_CLIENT_H
