#include "dsp.h"
#include <math.h>

// ESP-DSP library for hardware-accelerated FFT
#include "esp_dsp.h"
#include "dsps_fft2r.h"
#include "dsps_wind_hann.h"

// Global instance
DSP dsp;

bool DSP::begin() {
    // Initialize ESP-DSP FFT tables
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK) {
        Serial.printf("[DSP] FFT init failed: %d\n", ret);
        return false;
    }
    
    Serial.println("[DSP] Initialized successfully");
    return true;
}

void DSP::designButterworth(float cutoffHz, float sampleRateHz, uint8_t order) {
    // Normalize cutoff frequency
    float nyquist = sampleRateHz / 2.0f;
    float wn = cutoffHz / nyquist;
    
    // Limit to valid range
    if (wn >= 1.0f) wn = 0.99f;
    if (wn <= 0.0f) wn = 0.01f;
    
    // Pre-warp frequency for bilinear transform
    float fs = 2.0f;
    float warped = 2.0f * fs * tanf(M_PI * wn / fs);
    
    // Calculate number of second-order sections
    _numSections = (order + 1) / 2;
    if (_numSections > MAX_SOS) _numSections = MAX_SOS;
    
    // Design each second-order section
    for (int k = 0; k < _numSections; k++) {
        // Analog prototype pole
        float theta = M_PI * (2.0f * k + order + 1) / (2.0f * order);
        float real_s = warped * cosf(theta);
        float imag_s = warped * sinf(theta);
        
        // Bilinear transform: s = 2*fs*(z-1)/(z+1)
        // For lowpass: H(s) = w0^2 / (s^2 - 2*real_s*s + |s|^2)
        //
        // Using pre-computed coefficients for 4th order Butterworth
        // This is a simplified implementation
        float w0 = tanf(M_PI * cutoffHz / sampleRateHz);
        float k1 = sqrtf(2.0f) * w0;
        float k2 = w0 * w0;
        float a0 = 1.0f + k1 + k2;
        
        // Adjust coefficients for each section
        float q = 1.0f / (2.0f * cosf(M_PI * (2.0f * k + 1) / (2.0f * order)));
        k1 = w0 / q;
        a0 = 1.0f + k1 + k2;
        
        // Normalized coefficients
        _sos[k][0] = k2 / a0;           // b0
        _sos[k][1] = 2.0f * k2 / a0;    // b1
        _sos[k][2] = k2 / a0;           // b2
        _sos[k][3] = 2.0f * (k2 - 1.0f) / a0;    // a1
        _sos[k][4] = (1.0f - k1 + k2) / a0;      // a2
    }
    
    _resetState();
    
    Serial.printf("[DSP] Butterworth filter designed: %.1f Hz cutoff, order %d, %d sections\n",
                  cutoffHz, order, _numSections);
}

void DSP::_resetState() {
    for (int i = 0; i < MAX_SOS; i++) {
        _state[i][0] = 0.0f;
        _state[i][1] = 0.0f;
    }
}

float DSP::_processSample(float x, int section) {
    // Direct Form II Transposed
    float* b = _sos[section];
    float* z = _state[section];
    
    float y = b[0] * x + z[0];
    z[0] = b[1] * x - b[3] * y + z[1];
    z[1] = b[2] * x - b[4] * y;
    
    return y;
}

void DSP::applyFilter(float* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        float x = data[i];
        for (int s = 0; s < _numSections; s++) {
            x = _processSample(x, s);
        }
        data[i] = x;
    }
}

void DSP::_reverseArray(float* data, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        float temp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = temp;
    }
}

void DSP::applyFiltFilt(float* data, size_t len) {
    // Forward pass
    _resetState();
    applyFilter(data, len);
    
    // Reverse data
    _reverseArray(data, len);
    
    // Backward pass
    _resetState();
    applyFilter(data, len);
    
    // Reverse back to original order
    _reverseArray(data, len);
}

size_t DSP::computeFFT(float* input, float* output, size_t len, float sampleRateHz) {
    // Ensure length is power of 2
    size_t fftLen = nextPowerOf2(len);
    
    // Allocate interleaved complex array (real, imag pairs)
    float* fftData = (float*)malloc(fftLen * 2 * sizeof(float));
    if (!fftData) {
        Serial.println("[DSP] FFT malloc failed!");
        return 0;
    }
    
    // Copy input and zero-pad if necessary
    for (size_t i = 0; i < fftLen; i++) {
        if (i < len) {
            fftData[i * 2] = input[i];      // Real part
        } else {
            fftData[i * 2] = 0.0f;          // Zero-padding
        }
        fftData[i * 2 + 1] = 0.0f;          // Imaginary part
    }
    
    // Apply Hann window
    float* window = (float*)malloc(fftLen * sizeof(float));
    if (window) {
        dsps_wind_hann_f32(window, fftLen);
        for (size_t i = 0; i < fftLen; i++) {
            fftData[i * 2] *= window[i];
        }
        free(window);
    }
    
    // Perform FFT
    dsps_fft2r_fc32(fftData, fftLen);
    
    // Bit-reverse
    dsps_bit_rev_fc32(fftData, fftLen);
    
    // Calculate magnitude spectrum (single-sided)
    size_t numBins = fftLen / 2 + 1;
    float scale = 2.0f / (float)fftLen;
    
    for (size_t i = 0; i < numBins; i++) {
        float real = fftData[i * 2];
        float imag = fftData[i * 2 + 1];
        output[i] = sqrtf(real * real + imag * imag) * scale;
    }
    
    // DC and Nyquist components are not doubled
    output[0] /= 2.0f;
    if (numBins > 1) {
        output[numBins - 1] /= 2.0f;
    }
    
    free(fftData);
    
    return numBins;
}

float DSP::binToFrequency(size_t binIndex, size_t numSamples, float sampleRateHz) {
    return (float)binIndex * sampleRateHz / (float)numSamples;
}

size_t DSP::nextPowerOf2(size_t n) {
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}
