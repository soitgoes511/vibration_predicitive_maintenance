/**
 * Vibration Sensor Configuration - JavaScript
 * Handles API communication and form interactions
 */

// State
let currentConfig = {};
let statusInterval = null;

// DOM Elements
const elements = {
    deviceId: document.getElementById('device-id'),
    statusBanner: document.getElementById('status-banner'),
    statusText: document.getElementById('status-text'),

    // Status
    wifiStatus: document.getElementById('wifi-status'),
    ipAddress: document.getElementById('ip-address'),
    wifiRssi: document.getElementById('wifi-rssi'),
    triggerCount: document.getElementById('trigger-count'),
    influxStatus: document.getElementById('influx-status'),
    freeHeap: document.getElementById('free-heap'),

    // WiFi
    wifiSsid: document.getElementById('wifi-ssid'),
    wifiPassword: document.getElementById('wifi-password'),

    // InfluxDB
    influxUrl: document.getElementById('influx-url'),
    influxToken: document.getElementById('influx-token'),
    influxOrg: document.getElementById('influx-org'),
    influxBucket: document.getElementById('influx-bucket'),

    // Operation
    operationId: document.getElementById('operation-id'),

    // Hardware
    plcTriggerPin: document.getElementById('plc-trigger-pin'),
    spiCsPin: document.getElementById('spi-cs-pin'),

    // Sensor
    sensitivity: document.getElementById('sensitivity'),

    // Sampling
    sampleCount: document.getElementById('sample-count'),
    sampleRate: document.getElementById('sample-rate'),
    filterCutoff: document.getElementById('filter-cutoff'),
    sendTimeDomain: document.getElementById('send-time-domain'),

    // Buttons
    btnSave: document.getElementById('btn-save'),
    btnTestInflux: document.getElementById('btn-test-influx'),
    btnTrigger: document.getElementById('btn-trigger'),
    btnReset: document.getElementById('btn-reset')
};

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    loadConfig();
    updateStatus();

    // Update status every 5 seconds
    statusInterval = setInterval(updateStatus, 5000);

    // Event listeners
    elements.btnSave.addEventListener('click', saveConfig);
    elements.btnTestInflux.addEventListener('click', testInfluxConnection);
    elements.btnTrigger.addEventListener('click', manualTrigger);
    elements.btnReset.addEventListener('click', factoryReset);
});

// Show status banner
function showBanner(message, type = 'info') {
    elements.statusBanner.className = `status-banner ${type}`;
    elements.statusText.textContent = message;
    elements.statusBanner.classList.remove('hidden');

    // Auto-hide after 5 seconds for success messages
    if (type === 'success') {
        setTimeout(() => {
            elements.statusBanner.classList.add('hidden');
        }, 5000);
    }
}

// Hide status banner
function hideBanner() {
    elements.statusBanner.classList.add('hidden');
}

// Load configuration from API
async function loadConfig() {
    try {
        const response = await fetch('/api/config');
        if (!response.ok) throw new Error('Failed to load config');

        currentConfig = await response.json();
        populateForm(currentConfig);

    } catch (error) {
        console.error('Error loading config:', error);
        showBanner('Failed to load configuration', 'error');
    }
}

// Populate form with config values
function populateForm(config) {
    elements.deviceId.textContent = `Device: ${config.device_id || 'Unknown'}`;

    elements.wifiSsid.value = config.wifi_ssid || '';
    // Password not returned from API for security

    elements.influxUrl.value = config.influx_url || '';
    // Token not returned from API for security
    elements.influxOrg.value = config.influx_org || '';
    elements.influxBucket.value = config.influx_bucket || '';

    elements.operationId.value = config.operation_id || '';

    elements.plcTriggerPin.value = config.plc_trigger_pin || 4;
    elements.spiCsPin.value = config.spi_cs_pin || 5;

    elements.sensitivity.value = config.sensitivity || 2;

    elements.sampleCount.value = config.sample_count || 4000;
    elements.sampleRate.value = config.sample_rate_hz || 3200;
    elements.filterCutoff.value = config.filter_cutoff_hz || 1600;
    elements.sendTimeDomain.checked = config.send_time_domain || false;
}

// Update status display
async function updateStatus() {
    try {
        const response = await fetch('/api/status');
        if (!response.ok) throw new Error('Failed to get status');

        const status = await response.json();

        // WiFi status
        if (status.wifi_ap_mode) {
            elements.wifiStatus.textContent = 'AP Mode';
            elements.wifiStatus.className = 'value warning';
        } else if (status.wifi_connected) {
            elements.wifiStatus.textContent = 'Connected';
            elements.wifiStatus.className = 'value ok';
        } else {
            elements.wifiStatus.textContent = 'Disconnected';
            elements.wifiStatus.className = 'value error';
        }

        elements.ipAddress.textContent = status.wifi_ip || '--';

        // RSSI
        const rssi = status.wifi_rssi;
        if (rssi && rssi !== 0) {
            elements.wifiRssi.textContent = `${rssi} dBm`;
            elements.wifiRssi.className = rssi > -60 ? 'value ok' :
                rssi > -80 ? 'value warning' : 'value error';
        } else {
            elements.wifiRssi.textContent = '--';
            elements.wifiRssi.className = 'value';
        }

        // Trigger count
        elements.triggerCount.textContent = status.trigger_count || 0;

        // InfluxDB status
        if (!status.influx_configured) {
            elements.influxStatus.textContent = 'Not configured';
            elements.influxStatus.className = 'value warning';
        } else if (status.influx_last_ok) {
            elements.influxStatus.textContent = 'OK';
            elements.influxStatus.className = 'value ok';
        } else {
            elements.influxStatus.textContent = 'Error';
            elements.influxStatus.className = 'value error';
        }

        // Free heap
        const heap = status.free_heap;
        if (heap) {
            const heapKB = (heap / 1024).toFixed(0);
            elements.freeHeap.textContent = `${heapKB} KB`;
            elements.freeHeap.className = heap > 50000 ? 'value ok' :
                heap > 20000 ? 'value warning' : 'value error';
        }

    } catch (error) {
        console.error('Error updating status:', error);
    }
}

// Save configuration
async function saveConfig() {
    const config = {
        wifi_ssid: elements.wifiSsid.value,
        wifi_password: elements.wifiPassword.value,

        influx_url: elements.influxUrl.value,
        influx_token: elements.influxToken.value,
        influx_org: elements.influxOrg.value,
        influx_bucket: elements.influxBucket.value,

        operation_id: elements.operationId.value,

        plc_trigger_pin: parseInt(elements.plcTriggerPin.value),
        spi_cs_pin: parseInt(elements.spiCsPin.value),

        sensitivity: parseInt(elements.sensitivity.value),

        sample_count: parseInt(elements.sampleCount.value),
        sample_rate_hz: parseInt(elements.sampleRate.value),
        filter_cutoff_hz: parseInt(elements.filterCutoff.value),
        send_time_domain: elements.sendTimeDomain.checked
    };

    elements.btnSave.disabled = true;
    elements.btnSave.textContent = '💾 Saving...';

    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });

        const result = await response.json();

        if (result.success) {
            showBanner('Configuration saved! Device will reboot...', 'success');
        } else {
            showBanner(result.message || 'Failed to save', 'error');
        }

    } catch (error) {
        console.error('Error saving config:', error);
        showBanner('Failed to save configuration', 'error');
    } finally {
        elements.btnSave.disabled = false;
        elements.btnSave.textContent = '💾 Save Configuration';
    }
}

// Test InfluxDB connection
async function testInfluxConnection() {
    elements.btnTestInflux.disabled = true;
    elements.btnTestInflux.textContent = '🔗 Testing...';

    try {
        const response = await fetch('/api/test-influx', { method: 'POST' });
        const result = await response.json();

        if (result.success) {
            showBanner('InfluxDB connection successful!', 'success');
        } else {
            showBanner(`InfluxDB error: ${result.error}`, 'error');
        }

    } catch (error) {
        console.error('Error testing InfluxDB:', error);
        showBanner('Failed to test connection', 'error');
    } finally {
        elements.btnTestInflux.disabled = false;
        elements.btnTestInflux.textContent = '🔗 Test Connection';
    }
}

// Manual trigger
async function manualTrigger() {
    elements.btnTrigger.disabled = true;
    elements.btnTrigger.textContent = '▶️ Triggering...';

    try {
        const response = await fetch('/api/trigger', { method: 'POST' });
        const result = await response.json();

        if (result.success) {
            showBanner('Trigger initiated! Sampling in progress...', 'info');
            // Update status more frequently for a moment
            setTimeout(updateStatus, 2000);
            setTimeout(updateStatus, 5000);
        } else {
            showBanner(result.message || 'Trigger failed', 'error');
        }

    } catch (error) {
        console.error('Error triggering:', error);
        showBanner('Failed to trigger measurement', 'error');
    } finally {
        elements.btnTrigger.disabled = false;
        elements.btnTrigger.textContent = '▶️ Manual Trigger';
    }
}

// Factory reset
async function factoryReset() {
    if (!confirm('Are you sure you want to reset to factory defaults?\n\nThis will erase all WiFi and InfluxDB settings.')) {
        return;
    }

    elements.btnReset.disabled = true;
    elements.btnReset.textContent = '🔄 Resetting...';

    try {
        const response = await fetch('/api/reset', { method: 'POST' });
        const result = await response.json();

        if (result.success) {
            showBanner('Reset complete! Device will reboot...', 'success');
        } else {
            showBanner(result.message || 'Reset failed', 'error');
        }

    } catch (error) {
        console.error('Error resetting:', error);
        showBanner('Failed to reset', 'error');
    } finally {
        elements.btnReset.disabled = false;
        elements.btnReset.textContent = '🔄 Factory Reset';
    }
}
