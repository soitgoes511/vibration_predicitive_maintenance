# ESP32 Vibration Predictive Maintenance System

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)

A high-speed vibration data acquisition system for predictive maintenance of equipment bearings, running on ESP32-WROOM-32 with ADXL313 accelerometer.

## 🎯 Features

- **High-speed sampling**: 3200 Hz 3-axis accelerometer data acquisition
- **Signal processing**: Butterworth low-pass filtering + FFT frequency analysis
- **Cloud upload**: Real-time data push to InfluxDB 2.x
- **WiFi captive portal**: Easy field configuration via smartphone
- **Web-based settings**: Configure all parameters through a browser
- **PLC trigger**: GPIO interrupt for synchronized measurements
- **Persistent storage**: Settings survive power cycles (NVS)

## 📋 Hardware Requirements

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-WROOM-32 |
| Accelerometer | ADXL313 (SPI interface) |
| Power | 3.3V regulated |
| PLC Interface | 3.3V logic level signal |

## 🔌 Wiring Diagram

```
                    ┌─────────────────┐
                    │   ESP32-WROOM   │
                    │                 │
    ┌───────────────┤─── GPIO 18 (CLK)│
    │   ┌───────────┤─── GPIO 19 (MISO)
    │   │   ┌───────┤─── GPIO 23 (MOSI)
    │   │   │   ┌───┤─── GPIO 5  (CS) │
    │   │   │   │   │                 │
    │   │   │   │   │     GPIO 4  ◄───┤─── PLC Trigger (3.3V)
    │   │   │   │   │                 │
    │   │   │   │   └─────────────────┘
    │   │   │   │
┌───┴───┴───┴───┴───┐
│      ADXL313      │
│   Accelerometer   │
│                   │
│  CLK SDO SDI CS   │
│  VDD         GND  │
└───┬───────────┬───┘
    │           │
   3.3V        GND
```

### Default Pin Mapping

| Function | GPIO | Notes |
|----------|------|-------|
| SPI MOSI | 23 | VSPI default |
| SPI MISO | 19 | VSPI default |
| SPI CLK | 18 | VSPI default |
| SPI CS | 5 | Configurable via web UI |
| PLC Trigger | 4 | Configurable, internal pull-down |

> ⚠️ **Important**: ESP32 GPIO is NOT 5V tolerant. If your PLC outputs 5V, use a voltage divider.

## 🚀 Quick Start

### Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32 USB drivers
- ADXL313 accelerometer module

### Build & Upload

```bash
# Clone the repository
git clone https://github.com/soitgoes511/vibration_predicitive_maintenance.git
cd vibration_predicitive_maintenance

# Build firmware
pio run

# Upload firmware
pio run --target upload

# Upload web files (optional, for full UI)
pio run --target uploadfs
```

### First-Time Setup

1. **Power on the ESP32** - it will create a WiFi access point
2. **Connect to `VibSensor_XXXX`** WiFi network (XXXX = unique device ID)
3. **Open browser** - captive portal should open automatically, or navigate to `http://192.168.4.1`
4. **Configure settings**:
   - WiFi network credentials
   - InfluxDB connection (URL, token, org, bucket)
   - Operation ID (e.g., `L9OP600`)
   - Sensitivity range (±0.5g to ±4g)
5. **Save & Reboot** - device connects to your network

## ⚙️ Configuration Options

| Setting | Default | Description |
|---------|---------|-------------|
| WiFi SSID | (none) | Your WiFi network name |
| WiFi Password | (none) | Your WiFi password |
| InfluxDB URL | http://192.168.1.100:8086 | InfluxDB server address |
| InfluxDB Token | (none) | API authentication token |
| InfluxDB Org | expertise | Organization name |
| InfluxDB Bucket | expertise | Data bucket |
| Operation ID | L9OP600 | Equipment operation identifier |
| Sensitivity | ±2g | Accelerometer range |
| Sample Count | 4000 | Samples per measurement |
| Filter Cutoff | 1600 Hz | Butterworth low-pass filter |

## 📊 Data Format

### Frequency Domain (`accelfreq` measurement)

```
accelfreq,operation=L9OP600,device_id=6A4F,run_id=6A4F-1739356800-42 frequencies=100.0,x_freq=0.0123,y_freq=0.0045,z_freq=0.0067 1739356800001000000
```

### Time Domain (`acceltime` measurement)

```
acceltime,operation=L9OP600,device_id=6A4F,run_id=6A4F-1739356800-42 x=0.123,y=0.456,z=0.789 1739356800000000000
```

### Run Metadata (`accelrunmeta` measurement)

```
accelrunmeta,operation=L9OP600,device_id=6A4F,run_id=6A4F-1739356800-42 sample_rate_hz=3200i,sample_count=4000i,fft_size=4096i,filter_cutoff_hz=1600i,range_g=2.000,send_time_domain=false,window="hann",fw="1.1.0" 1739356800000000000
```

`run_id` format is now `<device_id>-<epoch_seconds>-<sequence>`, and timestamps use SNTP-synchronized epoch nanoseconds.

## 🔧 API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Configuration web interface |
| `/api/config` | GET | Get current configuration |
| `/api/config` | POST | Update configuration |
| `/api/status` | GET | Device status |
| `/api/trigger` | POST | Manual trigger |
| `/api/test-influx` | POST | Test InfluxDB connection |
| `/api/reset` | POST | Factory reset |

## 📁 Project Structure

```
├── platformio.ini              # Build configuration
├── README.md                   # This file
├── data/                       # Web interface (LittleFS)
│   ├── index.html
│   ├── style.css
│   └── script.js
└── src/
    ├── main.cpp                # Application entry point
    ├── config.h                # Configuration structures
    ├── config_manager.cpp      # NVS persistent storage
    ├── wifi_manager.cpp        # WiFi with captive portal
    ├── web_server.cpp          # Async HTTP server
    ├── adxl313.cpp             # Accelerometer SPI driver
    ├── dsp.cpp                 # Butterworth filter + FFT
    └── influxdb_client.cpp     # InfluxDB 2.x HTTP client
```

## 📈 Memory Usage

```
RAM:   [==        ]  16.9% (55 KB / 328 KB)
Flash: [==========]  99.4% (1.3 MB / 1.3 MB)
```

## 🔍 Troubleshooting

### ADXL313 Not Detected
- Check SPI wiring (CLK, MISO, MOSI, CS)
- Verify 3.3V power supply
- Device ID should be `0xAD` in serial monitor

### WiFi Connection Issues
- Verify SSID and password
- Device falls back to AP mode after 3 failed attempts

### InfluxDB Write Failures
- Verify URL is reachable from ESP32 network
- Check token has write permission
- Use "Test Connection" in web UI

## 📜 License

MIT License - See [LICENSE](LICENSE) for details.

## 🙏 Acknowledgments

- Originally developed for Raspberry Pi, ported to ESP32 for cost reduction
- Uses [ESP-DSP](https://github.com/espressif/esp-dsp) for hardware-accelerated FFT
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) for async HTTP handling
