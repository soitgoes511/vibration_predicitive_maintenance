#ifndef DSP_H
#define DSP_H

#include <Arduino.h>

/**
 * @brief Digital Signal Processing functions for vibration analysis
 * 
 * Implements Butterworth low-pass filtering and FFT for frequency analysis,
 * equivalent to the scipy.signal and scipy.fft Python functions.
 */
class DSP {
public:
    /**
     * @brief Initialize DSP module
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Design Butterworth low-pass filter coefficients
     * @param cutoffHz Cutoff frequency in Hz
     * @param sampleRateHz Sample rate in Hz
     * @param order Filter order (default 4)
     */
    void designButterworth(float cutoffHz, float sampleRateHz, uint8_t order = 4);
    
    /**
     * @brief Apply Butterworth low-pass filter to data (in-place)
     * @param data Array of samples (modified in place)
     * @param len Number of samples
     */
    void applyFilter(float* data, size_t len);
    
    /**
     * @brief Apply forward-backward filtering (like scipy filtfilt)
     * 
     * Applies filter forward then backward to achieve zero phase shift.
     * @param data Array of samples (modified in place)
     * @param len Number of samples
     */
    void applyFiltFilt(float* data, size_t len);
    
    /**
     * @brief Compute Real FFT magnitude spectrum
     * 
     * Computes single-sided amplitude spectrum from real input data.
     * @param input Input time-domain data (will be modified!)
     * @param output Output frequency-domain magnitudes
     * @param len Number of input samples (zero-padded internally to next power of 2)
     * @param sampleRateHz Sample rate for frequency calculation
     * @return Number of frequency bins (len/2 + 1)
     */
    size_t computeFFT(float* input, float* output, size_t len, float sampleRateHz);
    
    /**
     * @brief Get frequency value for a given FFT bin
     * @param binIndex Index of the frequency bin
     * @param numSamples Total number of samples used in FFT
     * @param sampleRateHz Sample rate in Hz
     * @return Frequency in Hz
     */
    static float binToFrequency(size_t binIndex, size_t numSamples, float sampleRateHz);
    
    /**
     * @brief Find the next power of 2 >= n
     * @param n Input value
     * @return Next power of 2
     */
    static size_t nextPowerOf2(size_t n);
    
private:
    // Butterworth filter coefficients (Second Order Sections for stability)
    static constexpr int MAX_ORDER = 4;
    static constexpr int MAX_SOS = MAX_ORDER / 2;  // Number of second-order sections
    
    // SOS coefficients: each section has [b0, b1, b2, a1, a2] (a0 = 1)
    float _sos[MAX_SOS][5];
    int _numSections;
    
    // Filter state for each section
    float _state[MAX_SOS][2];
    
    void _resetState();
    float _processSample(float x, int section);
    void _reverseArray(float* data, size_t len);
};

// Global instance
extern DSP dsp;

#endif // DSP_H
