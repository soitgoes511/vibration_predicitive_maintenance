#include "web_server.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "influxdb_client.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Global instance
WebServer webServer;

void WebServer::begin(uint16_t port) {
    if (_server) {
        delete _server;
    }
    
    _server = new AsyncWebServer(port);
    
    // Initialize LittleFS for static files
    if (!LittleFS.begin(true)) {
        Serial.println("[WebServer] LittleFS mount failed!");
    } else {
        Serial.println("[WebServer] LittleFS mounted");
    }
    
    _setupRoutes();
    _server->begin();
    
    Serial.printf("[WebServer] Started on port %d\n", port);
}

void WebServer::stop() {
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
    }
}

void WebServer::setTriggerCallback(void (*callback)()) {
    _triggerCallback = callback;
}

void WebServer::updateStatus(unsigned long lastTriggerTime, size_t sampleCount, bool influxOk) {
    _lastTriggerTime = lastTriggerTime;
    _lastSampleCount = sampleCount;
    _lastInfluxOk = influxOk;
    if (lastTriggerTime > 0) _triggerCount++;
}

void WebServer::_setupRoutes() {
    // Serve static files from LittleFS
    _server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // API endpoints
    _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleGetConfig(request);
    });
    
    // POST config with body handler
    _server->on("/api/config", HTTP_POST, 
        [](AsyncWebServerRequest* request) {},  // Request handler (empty)
        nullptr,  // Upload handler
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            _handlePostConfig(request, data, len);
        }
    );
    
    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleGetStatus(request);
    });
    
    _server->on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _handleReset(request);
    });
    
    _server->on("/api/trigger", HTTP_POST, [this](AsyncWebServerRequest* request) {
        _handleTrigger(request);
    });
    
    _server->on("/api/test-influx", HTTP_POST, [](AsyncWebServerRequest* request) {
        bool ok = influxClient.testConnection();
        StaticJsonDocument<256> doc;
        doc["success"] = ok;
        doc["error"] = ok ? "" : influxClient.getLastError();
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    // Captive portal detection endpoints
    _server->on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    _server->on("/fwlink", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    _server->on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    _server->on("/canonical.html", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    _server->on("/success.txt", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    _server->on("/ncsi.txt", HTTP_GET, [this](AsyncWebServerRequest* request) {
        _handleCaptivePortal(request);
    });
    
    // Fallback root handler if LittleFS fails
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            // Inline fallback HTML if filesystem not uploaded
            String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Vibration Sensor Setup</title>
    <style>
        body { font-family: -apple-system, sans-serif; background: #1a1a2e; color: #f1f5f9; 
               padding: 20px; max-width: 500px; margin: 0 auto; }
        h1 { color: #3b82f6; }
        .card { background: #1e293b; padding: 20px; border-radius: 12px; margin: 20px 0; }
        label { display: block; margin: 10px 0 5px; color: #94a3b8; }
        input { width: 100%; padding: 12px; border-radius: 8px; border: 1px solid #334155;
                background: #0f172a; color: #f1f5f9; box-sizing: border-box; }
        button { width: 100%; padding: 14px; border-radius: 8px; border: none; margin-top: 20px;
                 background: linear-gradient(135deg, #3b82f6, #2563eb); color: white; 
                 font-weight: bold; cursor: pointer; }
        .warn { background: #7c2d12; padding: 10px; border-radius: 8px; margin-bottom: 20px; }
    </style>
</head>
<body>
    <h1>🔧 Vibration Sensor</h1>
    <div class="warn">⚠️ Web UI files not found. Using basic setup page.</div>
    
    <form id="setupForm">
        <div class="card">
            <h2>📶 WiFi</h2>
            <label>SSID</label>
            <input type="text" id="wifi_ssid" required>
            <label>Password</label>
            <input type="password" id="wifi_password">
        </div>
        
        <div class="card">
            <h2>📈 InfluxDB</h2>
            <label>URL</label>
            <input type="text" id="influx_url" placeholder="http://192.168.1.100:8086">
            <label>Token</label>
            <input type="password" id="influx_token">
            <label>Org</label>
            <input type="text" id="influx_org" value="expertise">
            <label>Bucket</label>
            <input type="text" id="influx_bucket" value="expertise">
        </div>
        
        <div class="card">
            <h2>🏭 Operation</h2>
            <label>Operation ID</label>
            <input type="text" id="operation_id" placeholder="L9OP600">
        </div>
        
        <div class="card">
            <h2>📐 Sensor</h2>
            <label>Sensitivity Range</label>
            <select id="sensitivity" style="width:100%;padding:12px;border-radius:8px;border:1px solid #334155;background:#0f172a;color:#f1f5f9;">
                <option value="0">±0.5g (High resolution)</option>
                <option value="1">±1g</option>
                <option value="2" selected>±2g (Default)</option>
                <option value="3">±4g (High amplitude)</option>
            </select>
        </div>
        
        <button type="submit">💾 Save & Reboot</button>
    </form>
    
    <script>
        document.getElementById('setupForm').onsubmit = async (e) => {
            e.preventDefault();
            const config = {
                wifi_ssid: document.getElementById('wifi_ssid').value,
                wifi_password: document.getElementById('wifi_password').value,
                influx_url: document.getElementById('influx_url').value,
                influx_token: document.getElementById('influx_token').value,
                influx_org: document.getElementById('influx_org').value,
                influx_bucket: document.getElementById('influx_bucket').value,
                operation_id: document.getElementById('operation_id').value,
                sensitivity: parseInt(document.getElementById('sensitivity').value)
            };
            try {
                const resp = await fetch('/api/config', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(config)
                });
                alert('Saved! Device will reboot...');
            } catch(err) {
                alert('Error: ' + err);
            }
        };
    </script>
</body>
</html>
)rawliteral";
            request->send(200, "text/html", html);
        }
    });
    
    // 404 handler - serve fallback or 404
    _server->onNotFound([this](AsyncWebServerRequest* request) {
        // Don't redirect - just serve the status API or 404
        if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
        } else if (wifiManager.isAPMode()) {
            // For captive portal, redirect to root (which has fallback)
            request->redirect("http://192.168.4.1/");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });
}

void WebServer::_handleCaptivePortal(AsyncWebServerRequest* request) {
    if (wifiManager.isAPMode()) {
        request->redirect("http://" + wifiManager.getIP() + "/");
    } else {
        request->send(204);
    }
}

void WebServer::_handleGetConfig(AsyncWebServerRequest* request) {
    request->send(200, "application/json", _generateConfigJson());
}

String WebServer::_generateConfigJson() {
    DeviceConfig& cfg = configManager.getConfig();
    StaticJsonDocument<1024> doc;
    
    // WiFi
    doc["wifi_ssid"] = cfg.wifi_ssid;
    doc["wifi_password"] = "";  // Don't send password back
    
    // InfluxDB
    doc["influx_url"] = cfg.influx_url;
    doc["influx_token"] = "";  // Don't send token back
    doc["influx_org"] = cfg.influx_org;
    doc["influx_bucket"] = cfg.influx_bucket;
    
    // Operation
    doc["operation_id"] = cfg.operation_id;
    
    // Hardware
    doc["plc_trigger_pin"] = cfg.plc_trigger_pin;
    doc["spi_cs_pin"] = cfg.spi_cs_pin;
    
    // Sensor
    doc["sensitivity"] = cfg.sensitivity;
    
    // Sampling
    doc["sample_count"] = cfg.sample_count;
    doc["sample_rate_hz"] = cfg.sample_rate_hz;
    doc["filter_cutoff_hz"] = cfg.filter_cutoff_hz;
    doc["send_time_domain"] = cfg.send_time_domain;
    
    // Device info
    doc["device_id"] = configManager.getDeviceId();
    
    String json;
    serializeJson(doc, json);
    return json;
}

void WebServer::_handlePostConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len) {
    String json = String((char*)data).substring(0, len);
    
    if (_parseConfigJson(json)) {
        if (configManager.save()) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved. Rebooting...\"}");
            
            // Reboot to apply changes
            delay(1000);
            ESP.restart();
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
        }
    } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
    }
}

bool WebServer::_parseConfigJson(const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("[WebServer] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    DeviceConfig& cfg = configManager.getConfig();
    
    // WiFi (only update if provided)
    if (doc.containsKey("wifi_ssid") && strlen(doc["wifi_ssid"]) > 0) {
        strlcpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid));
    }
    if (doc.containsKey("wifi_password") && strlen(doc["wifi_password"]) > 0) {
        strlcpy(cfg.wifi_password, doc["wifi_password"], sizeof(cfg.wifi_password));
    }
    
    // InfluxDB
    if (doc.containsKey("influx_url")) {
        strlcpy(cfg.influx_url, doc["influx_url"], sizeof(cfg.influx_url));
    }
    if (doc.containsKey("influx_token") && strlen(doc["influx_token"]) > 0) {
        strlcpy(cfg.influx_token, doc["influx_token"], sizeof(cfg.influx_token));
    }
    if (doc.containsKey("influx_org")) {
        strlcpy(cfg.influx_org, doc["influx_org"], sizeof(cfg.influx_org));
    }
    if (doc.containsKey("influx_bucket")) {
        strlcpy(cfg.influx_bucket, doc["influx_bucket"], sizeof(cfg.influx_bucket));
    }
    
    // Operation
    if (doc.containsKey("operation_id")) {
        strlcpy(cfg.operation_id, doc["operation_id"], sizeof(cfg.operation_id));
    }
    
    // Hardware
    if (doc.containsKey("plc_trigger_pin")) {
        cfg.plc_trigger_pin = doc["plc_trigger_pin"];
    }
    if (doc.containsKey("spi_cs_pin")) {
        cfg.spi_cs_pin = doc["spi_cs_pin"];
    }
    
    // Sensor
    if (doc.containsKey("sensitivity")) {
        cfg.sensitivity = doc["sensitivity"];
        if (cfg.sensitivity > 3) cfg.sensitivity = 3;
    }
    
    // Sampling
    if (doc.containsKey("sample_count")) {
        cfg.sample_count = doc["sample_count"];
        if (cfg.sample_count > MAX_SAMPLE_COUNT) cfg.sample_count = MAX_SAMPLE_COUNT;
    }
    if (doc.containsKey("sample_rate_hz")) {
        cfg.sample_rate_hz = doc["sample_rate_hz"];
    }
    if (doc.containsKey("filter_cutoff_hz")) {
        cfg.filter_cutoff_hz = doc["filter_cutoff_hz"];
    }
    if (doc.containsKey("send_time_domain")) {
        cfg.send_time_domain = doc["send_time_domain"];
    }
    
    return true;
}

void WebServer::_handleGetStatus(AsyncWebServerRequest* request) {
    StaticJsonDocument<512> doc;
    
    doc["wifi_connected"] = wifiManager.isConnected();
    doc["wifi_ap_mode"] = wifiManager.isAPMode();
    doc["wifi_ip"] = wifiManager.getIP();
    doc["wifi_rssi"] = wifiManager.getRSSI();
    
    doc["influx_configured"] = configManager.isInfluxConfigured();
    doc["influx_last_ok"] = _lastInfluxOk;
    
    doc["trigger_count"] = _triggerCount;
    doc["last_trigger_time"] = _lastTriggerTime;
    doc["last_sample_count"] = _lastSampleCount;
    
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap"] = ESP.getFreeHeap();
    
    doc["device_id"] = configManager.getDeviceId();
    doc["ap_name"] = configManager.getAPName();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServer::_handleReset(AsyncWebServerRequest* request) {
    configManager.resetToDefaults();
    configManager.save();
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Reset to defaults. Rebooting...\"}");
    
    delay(1000);
    ESP.restart();
}

void WebServer::_handleTrigger(AsyncWebServerRequest* request) {
    if (_triggerCallback) {
        _triggerCallback();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Trigger initiated\"}");
    } else {
        request->send(503, "application/json", "{\"success\":false,\"message\":\"Trigger not available\"}");
    }
}
