#include "EnergyWebServer.h"
#include "RegisterDescriptors.h"
#include "EnergyAccumulator.h"
#include <WiFi.h>

extern const RegisterDescriptor registers[];
extern const uint16_t registerCount;

EnergyWebServer::EnergyWebServer(RegisterAccess& regAccess, uint16_t port)
  : _regAccess(regAccess), _settings(nullptr), _energyAccumulator(nullptr), _server(port), _settingsNeedReload(false) {
}
bool EnergyWebServer::begin(const char* ssid, const char* password) {
    // Store credentials for reconnect
    _lastSSID = String(ssid);
    _lastPassword = String(password);
    
    // Disconnect if already connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
    }
    
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to connect to WiFi");
        return false;
    }
    
    Serial.println("\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Setup routes
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/api/read", HTTP_GET, [this]() { handleReadRegister(); });
    _server.on("/api/read", HTTP_POST, [this]() { handleReadMultiple(); });
    _server.on("/api/write", HTTP_POST, [this]() { handleWriteRegister(); });
    _server.on("/api/write-multiple", HTTP_POST, [this]() { handleWriteMultiple(); });
    _server.on("/api/settings", HTTP_GET, [this]() { handleGetSettings(); });
    _server.on("/api/settings", HTTP_POST, [this]() { handleSetSettings(); });
    _server.on("/api/settings/calibration", HTTP_GET, [this]() { handleGetCalibration(); });
    _server.on("/api/settings/calibration", HTTP_POST, [this]() { handleSetCalibration(); });
    _server.on("/api/calibrate", HTTP_POST, [this]() { handleAutoCalibrate(); });
    _server.on("/api/settings/save", HTTP_POST, [this]() { handleSaveSettings(); });
    _server.on("/api/settings/reload", HTTP_POST, [this]() { handleReloadSettings(); });
    _server.on("/api/registers", HTTP_GET, [this]() { handleGetRegisters(); });
    _server.on("/api/energy", HTTP_GET, [this]() { handleGetEnergy(); });
    _server.on("/api/energy/calibrate/start", HTTP_POST, [this]() { handleStartEnergyCalibration(); });
    _server.on("/api/energy/calibrate/complete", HTTP_POST, [this]() { handleCompleteEnergyCalibration(); });
    _server.onNotFound([this]() { handleNotFound(); });
    
    // Enable CORS
    _server.enableCORS(true);
    
    _server.begin();
    Serial.println("HTTP server started");
    
    return true;
}

bool EnergyWebServer::reconnect() {
    if (_lastSSID.length() == 0) {
        Serial.println("No previous WiFi credentials stored");
        return false;
    }
    
    Serial.print("Reconnecting to WiFi");
    
    // Disconnect and reconnect WiFi only (don't restart server)
    WiFi.disconnect();
    delay(100);
    WiFi.begin(_lastSSID.c_str(), _lastPassword.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 5) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nFailed to reconnect to WiFi");
        return false;
    }
    
    Serial.println("\nReconnected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Server continues running with new IP
    return true;
}

void EnergyWebServer::handleClient() {
  _server.handleClient();
}

String EnergyWebServer::getIPAddress() {
  return WiFi.localIP().toString();
}

void EnergyWebServer::handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ATM90E32 Energy Monitor</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1000px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; }
        h2 { color: #666; margin-top: 30px; border-bottom: 2px solid #007bff; padding-bottom: 5px; }
        .endpoint { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 4px; border-left: 4px solid #007bff; }
        .method { display: inline-block; padding: 2px 8px; border-radius: 3px; font-weight: bold; margin-right: 10px; }
        .get { background: #28a745; color: white; }
        .post { background: #007bff; color: white; }
        code { background: #e9ecef; padding: 2px 6px; border-radius: 3px; font-family: monospace; }
        pre { background: #e9ecef; padding: 10px; border-radius: 4px; overflow-x: auto; font-size: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ATM90E32 Energy Monitor API</h1>
        
        <h2>Register Operations</h2>
        
        <div class="endpoint">
            <span class="method get">GET</span>
            <strong>/api/registers</strong>
            <p>Get list of all available registers with metadata</p>
            <pre>Response: {
  "success": true,
  "count": 250,
  "registers": [
    {
      "name": "UrmsA",
      "friendlyName": "Phase A RMS Voltage",
      "address": "49",
      "access": "read",
      "type": "uint16",
      "unit": "V",
      "scale": 0.01
    },
    ...
  ]
}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method get">GET</span>
            <strong>/api/read?name=UrmsA</strong>
            <p>Read a single register by name</p>
            <pre>Response: {"success": true, "name": "UrmsA", "value": 120.34}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/read</strong>
            <p>Read multiple registers at once</p>
            <pre>Request: {"registers": ["UrmsA", "IrmsA", "PmeanA"]}
Response: {
  "success": true,
  "data": [
    {"name": "UrmsA", "value": 120.34},
    {"name": "IrmsA", "value": 5.67},
    {"name": "PmeanA", "value": 682.73}
  ]
}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/write</strong>
            <p>Write a value to a register</p>
            <pre>Request: {"name": "MeterEn", "value": 1}
Response: {"success": true, "name": "MeterEn", "value": 1}</pre>
        </div>
        
        <h2>Settings Management</h2>
        
        <div class="endpoint">
            <span class="method get">GET</span>
            <strong>/api/settings</strong>
            <p>Get all current settings (WiFi, RTC, Timezone, DataLogging, Display, System, Calibration)</p>
            <pre>Response: {
  "success": true,
  "wifi": {"ssid": "MyNetwork"},
  "rtcCalibration": {...},
  "timezone": {...},
  "dataLogging": {...},
  "display": {...},
  "system": {...}
}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/settings</strong>
            <p>Update settings (partial updates supported - only include fields you want to change)</p>
            <pre>Request: {
  "dataLogging": {
    "loggingInterval": 5000,
    "bufferSize": 120
  },
  "display": {
    "field0": "UrmsA",
    "field1": "IrmsA"
  }
}
Response: {"success": true, "message": "Settings updated"}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/settings/save</strong>
            <p>Save current settings to SD card (settings.ini)</p>
            <pre>Response: {"success": true, "message": "Settings saved to SD card"}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/settings/reload</strong>
            <p>Reload settings from SD card</p>
            <pre>Response: {"success": true, "message": "Settings reloaded from SD card"}</pre>
        </div>
        
        <h2>Calibration</h2>
        
        <div class="endpoint">
            <span class="method get">GET</span>
            <strong>/api/settings/calibration</strong>
            <p>Get current calibration values</p>
            <pre>Response: {
  "success": true,
  "ugainA": "8000",
  "igainA": "7A00",
  ...
}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/settings/calibration</strong>
            <p>Set calibration values (hex strings)</p>
            <pre>Request: {
  "ugainA": "8000",
  "igainA": "7A00",
  "applyToChip": true
}
Response: {"success": true, "message": "Calibration updated and applied"}</pre>
        </div>
        
        <div class="endpoint">
            <span class="method post">POST</span>
            <strong>/api/calibrate</strong>
            <p>Automatic calibration - provide expected vs measured values</p>
            <pre>Request: {
  "phase": "A",
  "type": "voltage",
  "expected": 120.0,
  "measured": 115.2
}
Response: {
  "success": true,
  "phase": "A",
  "type": "voltage",
  "oldGain": "8000",
  "newGain": "8348",
  "ratio": 1.0417,
  "message": "Calibration calculated and applied"
}</pre>
        </div>
        
        <p><strong>Try it:</strong> Use tools like Postman, curl, Python requests, or fetch() in the browser console.</p>
        
        <h3>Example Python Usage:</h3>
        <pre>import requests

# Get all registers
regs = requests.get('http://192.168.1.100/api/registers').json()

# Read multiple values
data = requests.post('http://192.168.1.100/api/read',
    json={'registers': ['UrmsA', 'IrmsA', 'PmeanA']}).json()

# Update settings
requests.post('http://192.168.1.100/api/settings',
    json={'dataLogging': {'loggingInterval': 5000}})

# Save to SD card
requests.post('http://192.168.1.100/api/settings/save')

# Auto-calibrate
requests.post('http://192.168.1.100/api/calibrate',
    json={'phase': 'A', 'type': 'voltage', 'expected': 120.0, 'measured': 115.2})
</pre>
    </div>
</body>
</html>
)";
  _server.send(200, "text/html", html);
}

void EnergyWebServer::handleReadRegister() {
  if (!_server.hasArg("name")) {
    sendError(400, "Missing 'name' parameter");
    return;
  }

  String regName = _server.arg("name");
  bool success = false;
  float value = _regAccess.readRegister(regName.c_str(), &success);

  JsonDocument doc;

  if (success) {
    doc["success"] = true;
    doc["name"] = regName;
    doc["value"] = value;
    sendJSON(200, doc);
  } else {
    doc["success"] = false;
    doc["error"] = "Register not found or not readable";
    sendJSON(404, doc);
  }
}

void EnergyWebServer::handleReadMultiple() {
digitalWrite(2, HIGH);  // LED on during processing
  if (!_server.hasArg("plain")) {
    sendError(400, "No JSON body provided");
    return;
  }

  JsonDocument reqDoc;
  DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));

  if (error) {
    sendError(400, "Invalid JSON");
    return;
  }

  if (!reqDoc.containsKey("registers")) {
    sendError(400, "Missing 'registers' array in JSON");
    return;
  }

  JsonDocument resDoc;
  resDoc["success"] = true;
  JsonArray dataArray = resDoc["data"].to<JsonArray>();

  JsonArray registers = reqDoc["registers"].as<JsonArray>();
  for (JsonVariant reg : registers) {
    String regName = reg.as<String>();
    bool success = false;
    float value = _regAccess.readRegister(regName.c_str(), &success);

    JsonObject item = dataArray.add<JsonObject>();
    item["name"] = regName;

    if (success) {
      item["value"] = value;
    } else {
      item["error"] = "Not found or not readable";
    }
  }

  sendJSON(200, resDoc);
  digitalWrite(2, LOW);  // LED off when done
}

void EnergyWebServer::handleWriteRegister() {
  if (!_server.hasArg("plain")) {
    sendError(400, "No JSON body provided");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, _server.arg("plain"));

  if (error) {
    sendError(400, "Invalid JSON");
    return;
  }

  if (!doc.containsKey("name") || !doc.containsKey("value")) {
    sendError(400, "Missing 'name' or 'value' in JSON");
    return;
  }

  String regName = doc["name"].as<String>();
  float value = doc["value"].as<float>();

  bool success = _regAccess.writeRegister(regName.c_str(), value);

  JsonDocument resDoc;

  if (success) {
    // Read back the value
    bool readSuccess = false;
    float readValue = _regAccess.readRegister(regName.c_str(), &readSuccess);

    resDoc["success"] = true;
    resDoc["name"] = regName;
    resDoc["value"] = readValue;
    sendJSON(200, resDoc);
  } else {
    resDoc["success"] = false;
    resDoc["error"] = "Register not found or not writable";
    sendJSON(404, resDoc);
  }
}

void EnergyWebServer::handleWriteMultiple() {
    if (!_server.hasArg("plain")) {
        sendError(400, "No JSON body provided");
        return;
    }
    
    StaticJsonDocument<2048> reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    if (!reqDoc.containsKey("writes")) {
        sendError(400, "Missing 'writes' array in JSON");
        return;
    }
    
    StaticJsonDocument<2048> resDoc;
    resDoc["success"] = true;
    JsonArray resultsArray = resDoc["results"].to<JsonArray>();
    
    JsonArray writes = reqDoc["writes"].as<JsonArray>();
    for (JsonVariant write : writes) {
        if (!write.containsKey("name") || !write.containsKey("value")) {
            JsonObject result = resultsArray.add<JsonObject>();
            result["error"] = "Missing name or value";
            continue;
        }
        
        String regName = write["name"].as<String>();
        float value = write["value"].as<float>();
        
        bool writeSuccess = _regAccess.writeRegister(regName.c_str(), value);
        
        JsonObject result = resultsArray.add<JsonObject>();
        result["name"] = regName;
        
        if (writeSuccess) {
            // Read back the value to confirm
            bool readSuccess = false;
            float readValue = _regAccess.readRegister(regName.c_str(), &readSuccess);
            
            result["success"] = true;
            result["value"] = readValue;
        } else {
            result["success"] = false;
            result["error"] = "Register not found or not writable";
        }
    }
    
    sendJSON(200, resDoc);
}

void EnergyWebServer::handleNotFound() {
  sendError(404, "Endpoint not found");
}

void EnergyWebServer::sendJSON(int code, JsonDocument& doc) {
  String response;
  serializeJson(doc, response);
  _server.send(code, "application/json", response);
}

void EnergyWebServer::sendError(int code, const char* message) {
  JsonDocument doc;
  doc["success"] = false;
  doc["error"] = message;
  sendJSON(code, doc);
}


void EnergyWebServer::handleGetSettings() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    
    JsonDocument doc;
    doc["success"] = true;
    
    // WiFi settings (omit password for security)
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = _settings->getWiFiSettings().ssid;
    
    // RTC Calibration
    JsonObject rtc = doc["rtcCalibration"].to<JsonObject>();
    const RTCCalibrationSettings& rtcCal = _settings->getRTCCalibration();
    rtc["ntpServer"] = rtcCal.ntpServer;
    rtc["minCalibrationDays"] = rtcCal.minCalibrationDays;
    rtc["calibrationThreshold"] = rtcCal.calibrationThreshold;
    rtc["autoCalibrationEnabled"] = rtcCal.autoCalibrationEnabled;
    rtc["calibrationEnabled"] = rtcCal.calibrationEnabled;
    rtc["lastCalibrationTime"] = rtcCal.lastCalibrationTime;
    rtc["currentOffset"] = rtcCal.currentOffset;
    
    // Timezone
    JsonObject tz = doc["timezone"].to<JsonObject>();
    const TimezoneSettings& timezone = _settings->getTimezoneSettings();
    tz["dstAbbrev"] = timezone.dstAbbrev;
    tz["dstWeek"] = timezone.dstWeek;
    tz["dstDow"] = timezone.dstDow;
    tz["dstMonth"] = timezone.dstMonth;
    tz["dstHour"] = timezone.dstHour;
    tz["dstOffset"] = timezone.dstOffset;
    tz["stdAbbrev"] = timezone.stdAbbrev;
    tz["stdWeek"] = timezone.stdWeek;
    tz["stdDow"] = timezone.stdDow;
    tz["stdMonth"] = timezone.stdMonth;
    tz["stdHour"] = timezone.stdHour;
    tz["stdOffset"] = timezone.stdOffset;
    
    // Data Logging
    JsonObject logging = doc["dataLogging"].to<JsonObject>();
    const DataLoggingSettings& log = _settings->getDataLoggingSettings();
    logging["loggingInterval"] = log.loggingInterval;
    logging["bufferSize"] = log.bufferSize;
    logging["powerLossThreshold"] = log.powerLossThreshold;
    logging["enablePowerLossDetection"] = log.enablePowerLossDetection;
    logging["logFields"] = log.logFields;
    
    // Display
    JsonObject display = doc["display"].to<JsonObject>();
    const DisplaySettings& disp = _settings->getDisplaySettings();
    display["field0"] = disp.field0;
    display["field1"] = disp.field1;
    display["field2"] = disp.field2;
    display["backlightTimeout"] = disp.backlightTimeout;
    display["longPressTime"] = disp.longPressTime;
    
    // System
    JsonObject system = doc["system"].to<JsonObject>();
    const SystemSettings& sys = _settings->getSystemSettings();
    system["autoRebootEnabled"] = sys.autoRebootEnabled;
    system["rebootIntervalHours"] = sys.rebootIntervalHours;
    system["rebootHour"] = sys.rebootHour;
    
    sendJSON(200, doc);
}

void EnergyWebServer::handleSetSettings() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    
    if (!_server.hasArg("plain")) {
        sendError(400, "No JSON body provided");
        return;
    }
    
    JsonDocument reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    // Update WiFi if provided
    if (reqDoc.containsKey("wifi")) {
        WiFiSettings wifi = _settings->getWiFiSettings();
        if (reqDoc["wifi"].containsKey("ssid")) {
            wifi.ssid = reqDoc["wifi"]["ssid"].as<String>();
        }
        if (reqDoc["wifi"].containsKey("password")) {
            wifi.password = reqDoc["wifi"]["password"].as<String>();
        }
        _settings->setWiFiSettings(wifi);
    }
    
    // Update RTC Calibration if provided
    if (reqDoc.containsKey("rtcCalibration")) {
        RTCCalibrationSettings rtc = _settings->getRTCCalibration();
        JsonObject rtcObj = reqDoc["rtcCalibration"];
        if (rtcObj.containsKey("ntpServer")) rtc.ntpServer = rtcObj["ntpServer"].as<String>();
        if (rtcObj.containsKey("minCalibrationDays")) rtc.minCalibrationDays = rtcObj["minCalibrationDays"];
        if (rtcObj.containsKey("calibrationThreshold")) rtc.calibrationThreshold = rtcObj["calibrationThreshold"];
        if (rtcObj.containsKey("autoCalibrationEnabled")) rtc.autoCalibrationEnabled = rtcObj["autoCalibrationEnabled"];
        if (rtcObj.containsKey("calibrationEnabled")) rtc.calibrationEnabled = rtcObj["calibrationEnabled"];
        _settings->setRTCCalibration(rtc);
    }
    
    // Update Timezone if provided
    if (reqDoc.containsKey("timezone")) {
        TimezoneSettings tz = _settings->getTimezoneSettings();
        JsonObject tzObj = reqDoc["timezone"];
        if (tzObj.containsKey("dstAbbrev")) tz.dstAbbrev = tzObj["dstAbbrev"].as<String>();
        if (tzObj.containsKey("dstWeek")) tz.dstWeek = tzObj["dstWeek"];
        if (tzObj.containsKey("dstDow")) tz.dstDow = tzObj["dstDow"];
        if (tzObj.containsKey("dstMonth")) tz.dstMonth = tzObj["dstMonth"];
        if (tzObj.containsKey("dstHour")) tz.dstHour = tzObj["dstHour"];
        if (tzObj.containsKey("dstOffset")) tz.dstOffset = tzObj["dstOffset"];
        if (tzObj.containsKey("stdAbbrev")) tz.stdAbbrev = tzObj["stdAbbrev"].as<String>();
        if (tzObj.containsKey("stdWeek")) tz.stdWeek = tzObj["stdWeek"];
        if (tzObj.containsKey("stdDow")) tz.stdDow = tzObj["stdDow"];
        if (tzObj.containsKey("stdMonth")) tz.stdMonth = tzObj["stdMonth"];
        if (tzObj.containsKey("stdHour")) tz.stdHour = tzObj["stdHour"];
        if (tzObj.containsKey("stdOffset")) tz.stdOffset = tzObj["stdOffset"];
        _settings->setTimezoneSettings(tz);
    }
    
    // Update Data Logging if provided
    if (reqDoc.containsKey("dataLogging")) {
        DataLoggingSettings log = _settings->getDataLoggingSettings();
        JsonObject logObj = reqDoc["dataLogging"];
        if (logObj.containsKey("loggingInterval")) log.loggingInterval = logObj["loggingInterval"];
        if (logObj.containsKey("bufferSize")) log.bufferSize = logObj["bufferSize"];
        if (logObj.containsKey("powerLossThreshold")) log.powerLossThreshold = logObj["powerLossThreshold"];
        if (logObj.containsKey("enablePowerLossDetection")) log.enablePowerLossDetection = logObj["enablePowerLossDetection"];
        if (logObj.containsKey("logFields")) log.logFields = logObj["logFields"].as<String>();
        _settings->setDataLoggingSettings(log);
    }
    
    // Update Display if provided
    if (reqDoc.containsKey("display")) {
        DisplaySettings disp = _settings->getDisplaySettings();
        JsonObject dispObj = reqDoc["display"];
        if (dispObj.containsKey("field0")) disp.field0 = dispObj["field0"].as<String>();
        if (dispObj.containsKey("field1")) disp.field1 = dispObj["field1"].as<String>();
        if (dispObj.containsKey("field2")) disp.field2 = dispObj["field2"].as<String>();
        if (dispObj.containsKey("backlightTimeout")) disp.backlightTimeout = dispObj["backlightTimeout"];
        if (dispObj.containsKey("longPressTime")) disp.longPressTime = dispObj["longPressTime"];
        _settings->setDisplaySettings(disp);
    }
    
    // Update System if provided
    if (reqDoc.containsKey("system")) {
        SystemSettings sys = _settings->getSystemSettings();
        JsonObject sysObj = reqDoc["system"];
        if (sysObj.containsKey("autoRebootEnabled")) sys.autoRebootEnabled = sysObj["autoRebootEnabled"];
        if (sysObj.containsKey("rebootIntervalHours")) sys.rebootIntervalHours = sysObj["rebootIntervalHours"];
        if (sysObj.containsKey("rebootHour")) sys.rebootHour = sysObj["rebootHour"];
        _settings->setSystemSettings(sys);
    }
    
    JsonDocument resDoc;
    resDoc["success"] = true;
    resDoc["message"] = "Settings updated in memory. Use /api/settings/save to persist to SD card.";
    sendJSON(200, resDoc);
    _settingsNeedReload = 1;
}

void EnergyWebServer::handleGetCalibration() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }

    const MeasurementCalibrationRegisters& meas = _settings->getMeasurementCalibrationRegisters();
    const CalibrationRegisters& cal = _settings->getCalibrationRegisters();

    JsonDocument doc;
    doc["success"] = true;

    // Voltage gains
    doc["ugainA"] = String(meas.UgainA, HEX);
    doc["ugainB"] = String(meas.UgainB, HEX);
    doc["ugainC"] = String(meas.UgainC, HEX);

    // Current gains
    doc["igainA"] = String(meas.IgainA, HEX);
    doc["igainB"] = String(meas.IgainB, HEX);
    doc["igainC"] = String(meas.IgainC, HEX);

    // Offsets
    doc["uoffsetA"] = String(meas.UoffsetA, HEX);
    doc["uoffsetB"] = String(meas.UoffsetB, HEX);
    doc["uoffsetC"] = String(meas.UoffsetC, HEX);
    doc["ioffsetA"] = String(meas.IoffsetA, HEX);
    doc["ioffsetB"] = String(meas.IoffsetB, HEX);
    doc["ioffsetC"] = String(meas.IoffsetC, HEX);
    doc["poffsetA"] = cal.PoffsetA;
    doc["poffsetB"] = cal.PoffsetB;
    doc["poffsetC"] = cal.PoffsetC;
    doc["qoffsetA"] = cal.QoffsetA;
    doc["qoffsetB"] = cal.QoffsetB;
    doc["qoffsetC"] = cal.QoffsetC;

    sendJSON(200, doc);
}

void EnergyWebServer::handleSetCalibration() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    
    if (!_server.hasArg("plain")) {
        sendError(400, "No JSON body provided");
        return;
    }
    
    JsonDocument reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    // Get current settings
    MeasurementCalibrationRegisters meas = _settings->getMeasurementCalibrationRegisters();
    CalibrationRegisters cal = _settings->getCalibrationRegisters();

    // Update voltage gains if provided (accept hex strings)
    if (reqDoc.containsKey("ugainA")) meas.UgainA = strtoul(reqDoc["ugainA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("ugainB")) meas.UgainB = strtoul(reqDoc["ugainB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("ugainC")) meas.UgainC = strtoul(reqDoc["ugainC"].as<const char*>(), NULL, 16);

    // Update current gains
    if (reqDoc.containsKey("igainA")) meas.IgainA = strtoul(reqDoc["igainA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("igainB")) meas.IgainB = strtoul(reqDoc["igainB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("igainC")) meas.IgainC = strtoul(reqDoc["igainC"].as<const char*>(), NULL, 16);

    // Update RMS offsets
    if (reqDoc.containsKey("uoffsetA")) meas.UoffsetA = strtoul(reqDoc["uoffsetA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("uoffsetB")) meas.UoffsetB = strtoul(reqDoc["uoffsetB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("uoffsetC")) meas.UoffsetC = strtoul(reqDoc["uoffsetC"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("ioffsetA")) meas.IoffsetA = strtoul(reqDoc["ioffsetA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("ioffsetB")) meas.IoffsetB = strtoul(reqDoc["ioffsetB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("ioffsetC")) meas.IoffsetC = strtoul(reqDoc["ioffsetC"].as<const char*>(), NULL, 16);

    // Update power offsets
    if (reqDoc.containsKey("poffsetA")) cal.PoffsetA = strtoul(reqDoc["poffsetA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("poffsetB")) cal.PoffsetB = strtoul(reqDoc["poffsetB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("poffsetC")) cal.PoffsetC = strtoul(reqDoc["poffsetC"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("qoffsetA")) cal.QoffsetA = strtoul(reqDoc["qoffsetA"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("qoffsetB")) cal.QoffsetB = strtoul(reqDoc["qoffsetB"].as<const char*>(), NULL, 16);
    if (reqDoc.containsKey("qoffsetC")) cal.QoffsetC = strtoul(reqDoc["qoffsetC"].as<const char*>(), NULL, 16);

    _settings->setMeasurementCalibrationRegisters(meas);
    _settings->setCalibrationRegisters(cal);

    // Apply to chip if requested
    bool applyToChip = reqDoc["applyToChip"] | false;
    if (applyToChip) {
        _settings->applyAllRegistersToChip();
    }
    
    JsonDocument resDoc;
    resDoc["success"] = true;
    resDoc["message"] = applyToChip ? "Calibration updated and applied to chip" : "Calibration updated (not applied to chip yet)";
    sendJSON(200, resDoc);
}

void EnergyWebServer::handleAutoCalibrate() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    
    if (!_server.hasArg("plain")) {
        sendError(400, "No JSON body provided");
        return;
    }
    
    JsonDocument reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));
    
    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }
    
    // Expected format:
    // {
    //   "phase": "A",  // or "B" or "C"
    //   "type": "voltage",  // or "current"
    //   "expected": 120.0,  // Expected voltage/current
    //   "measured": 115.2   // What the meter currently reads
    // }
    
    if (!reqDoc.containsKey("phase") || !reqDoc.containsKey("type") || 
        !reqDoc.containsKey("expected") || !reqDoc.containsKey("measured")) {
        sendError(400, "Missing required fields: phase, type, expected, measured");
        return;
    }
    
    String phase = reqDoc["phase"].as<String>();
    String type = reqDoc["type"].as<String>();
    float expected = reqDoc["expected"];
    float measured = reqDoc["measured"];
    
    if (measured == 0.0) {
        sendError(400, "Measured value cannot be zero");
        return;
    }
    
    // Calculate gain adjustment factor
    float ratio = expected / measured;
    
    // Get current measurement calibration
    MeasurementCalibrationRegisters meas = _settings->getMeasurementCalibrationRegisters();

    // Determine which gain to adjust
    uint16_t currentGain = 0x8000;  // Default nominal gain
    uint16_t* targetGain = nullptr;

    if (type == "voltage") {
        if (phase == "A") { currentGain = meas.UgainA; targetGain = &meas.UgainA; }
        else if (phase == "B") { currentGain = meas.UgainB; targetGain = &meas.UgainB; }
        else if (phase == "C") { currentGain = meas.UgainC; targetGain = &meas.UgainC; }
    } else if (type == "current") {
        if (phase == "A") { currentGain = meas.IgainA; targetGain = &meas.IgainA; }
        else if (phase == "B") { currentGain = meas.IgainB; targetGain = &meas.IgainB; }
        else if (phase == "C") { currentGain = meas.IgainC; targetGain = &meas.IgainC; }
    }

    if (targetGain == nullptr) {
        sendError(400, "Invalid phase or type");
        return;
    }

    // Calculate new gain: new_gain = current_gain * ratio
    uint32_t newGain = (uint32_t)(currentGain * ratio);

    // Clamp to valid range (0x0000 to 0xFFFF)
    if (newGain > 0xFFFF) newGain = 0xFFFF;
    if (newGain < 0x1000) newGain = 0x1000;  // Minimum reasonable gain

    *targetGain = (uint16_t)newGain;

    // Update settings
    _settings->setMeasurementCalibrationRegisters(meas);

    // Apply to chip
    bool applied = _settings->applyAllRegistersToChip();
    
    JsonDocument resDoc;
    resDoc["success"] = applied;
    resDoc["phase"] = phase;
    resDoc["type"] = type;
    resDoc["oldGain"] = String(currentGain, HEX);
    resDoc["newGain"] = String(newGain, HEX);
    resDoc["ratio"] = ratio;
    resDoc["message"] = applied ? "Calibration calculated and applied" : "Calibration calculated but failed to apply";
    
    sendJSON(200, resDoc);
}

void EnergyWebServer::handleSaveSettings() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    
    bool success = _settings->saveSettings();
    
    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Settings saved to SD card" : "Failed to save settings";
    sendJSON(success ? 200 : 500, doc);
}

void EnergyWebServer::handleReloadSettings() {
    if (!_settings) {
        sendError(500, "Settings manager not initialized");
        return;
    }
    Serial.println("Attempting to load settings from server...");
    bool success = _settings->loadSettings();
    _settingsNeedReload = 1;
    
    JsonDocument doc;
    doc["success"] = success;
    doc["message"] = success ? "Settings reloaded from SD card" : "Failed to reload settings";
    sendJSON(success ? 200 : 500, doc);
}

void EnergyWebServer::handleGetRegisters() {
    JsonDocument doc;
    doc["success"] = true;
    doc["count"] = registerCount;
    
    JsonArray regs = doc["registers"].to<JsonArray>();
    
    // Iterate through all available registers
    for (size_t i = 0; i < registerCount; i++) {
        JsonObject reg = regs.add<JsonObject>();
        
        reg["name"] = registers[i].name;
        reg["friendlyName"] = registers[i].friendlyName;
        reg["address"] = String(registers[i].address[0], HEX);
        
        // Access type
        switch (registers[i].rwType) {
            case RW_READ: reg["access"] = "read"; break;
            case RW_WRITE: reg["access"] = "write"; break;
            case RW_READWRITE: reg["access"] = "readwrite"; break;
            case RW_READWRITE1CLEAR: reg["access"] = "readwrite1clear"; break;
            case RW_READCLEAR: reg["access"] = "readclear"; break;
            default: reg["access"] = "unknown";
        }
        
        // Data type
        switch (registers[i].regType) {
            case DT_UINT8: reg["type"] = "uint8"; break;
            case DT_INT8: reg["type"] = "int8"; break;
            case DT_UINT16: reg["type"] = "uint16"; break;
            case DT_INT16: reg["type"] = "int16"; break;
            case DT_UINT32: reg["type"] = "uint32"; break;
            case DT_INT32: reg["type"] = "int32"; break;
            case DT_BIT: reg["type"] = "bit"; break;
            case DT_BITFIELD: reg["type"] = "bitfield"; break;
            default: reg["type"] = "unknown";
        }
        
        reg["unit"] = registers[i].unit ? registers[i].unit : "";
        reg["scale"] = registers[i].scale;
        
        // For bitfields, include bit position and length
        if (registers[i].regType == DT_BITFIELD || registers[i].regType == DT_BIT) {
            reg["bitPos"] = registers[i].bitPos;
            if (registers[i].regType == DT_BITFIELD) {
                reg["bitLen"] = registers[i].bitLen;
            }
        }
    }
    
    sendJSON(200, doc);
}

bool EnergyWebServer::settingsNeedReload(){
  bool result = _settingsNeedReload;
  _settingsNeedReload = false;  // Clear flag
  return result;
}

void EnergyWebServer::handleGetEnergy() {
    if (!_energyAccumulator) {
        sendError(500, "Energy accumulator not initialized");
        return;
    }

    // Get query parameter for phase (default to A)
    String phaseParam = "A";
    if (_server.hasArg("phase")) {
        phaseParam = _server.arg("phase");
        phaseParam.toUpperCase();
    }

    JsonDocument doc;
    doc["success"] = true;
    doc["readInterval"] = _energyAccumulator->getReadInterval();
    doc["saveInterval"] = _energyAccumulator->getSaveInterval();
    doc["lastReadTime"] = _energyAccumulator->getLastReadTime();
    doc["lastSaveTime"] = _energyAccumulator->getLastSaveTime();

    // Handle specific phase or all phases
    if (phaseParam == "A" || phaseParam == "B" || phaseParam == "C") {
        uint8_t phase = phaseParam.charAt(0) - 'A';  // Convert A/B/C to 0/1/2
        doc["phase"] = phaseParam;
        doc["accumulatedKWh"] = _energyAccumulator->getAccumulatedEnergy(phase);
    } else if (phaseParam == "ALL") {
        JsonArray phases = doc["phases"].to<JsonArray>();

        JsonObject phaseA = phases.add<JsonObject>();
        phaseA["phase"] = "A";
        phaseA["accumulatedKWh"] = _energyAccumulator->getAccumulatedEnergy(0);

        JsonObject phaseB = phases.add<JsonObject>();
        phaseB["phase"] = "B";
        phaseB["accumulatedKWh"] = _energyAccumulator->getAccumulatedEnergy(1);

        JsonObject phaseC = phases.add<JsonObject>();
        phaseC["phase"] = "C";
        phaseC["accumulatedKWh"] = _energyAccumulator->getAccumulatedEnergy(2);

        doc["totalKWh"] = _energyAccumulator->getAccumulatedEnergy(0) +
                         _energyAccumulator->getAccumulatedEnergy(1) +
                         _energyAccumulator->getAccumulatedEnergy(2);
    } else {
        sendError(400, "Invalid phase parameter. Use A, B, C, or ALL");
        return;
    }

    sendJSON(200, doc);
}

void EnergyWebServer::handleStartEnergyCalibration() {
    if (!_energyAccumulator) {
        sendError(500, "Energy accumulator not initialized");
        return;
    }

    // Parse JSON body
    JsonDocument reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));

    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }

    // Get phases to calibrate (default to A)
    uint8_t phaseMask = 0;
    if (reqDoc.containsKey("phases")) {
        String phasesStr = reqDoc["phases"].as<String>();
        phasesStr.toUpperCase();

        if (phasesStr.indexOf('A') >= 0) phaseMask |= 0x01;
        if (phasesStr.indexOf('B') >= 0) phaseMask |= 0x02;
        if (phasesStr.indexOf('C') >= 0) phaseMask |= 0x04;
    } else {
        // Default to phase A
        phaseMask = 0x01;
    }

    if (phaseMask == 0) {
        sendError(400, "No valid phases specified. Use 'phases': 'A', 'B', 'C', 'AB', 'AC', 'BC', or 'ABC'");
        return;
    }

    // Start calibration
    if (_energyAccumulator->startCalibration(phaseMask)) {
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Energy calibration started";
        doc["calibratingPhases"] = "";

        if (phaseMask & 0x01) doc["calibratingPhases"] = doc["calibratingPhases"].as<String>() + "A";
        if (phaseMask & 0x02) doc["calibratingPhases"] = doc["calibratingPhases"].as<String>() + "B";
        if (phaseMask & 0x04) doc["calibratingPhases"] = doc["calibratingPhases"].as<String>() + "C";

        sendJSON(200, doc);
    } else {
        sendError(500, "Failed to start energy calibration");
    }
}

void EnergyWebServer::handleCompleteEnergyCalibration() {
    if (!_energyAccumulator) {
        sendError(500, "Energy accumulator not initialized");
        return;
    }

    if (!_energyAccumulator->isCalibrating()) {
        sendError(400, "Not in calibration mode. Call /api/energy/calibrate/start first");
        return;
    }

    // Parse JSON body
    JsonDocument reqDoc;
    DeserializationError error = deserializeJson(reqDoc, _server.arg("plain"));

    if (error) {
        sendError(400, "Invalid JSON");
        return;
    }

    // Validate required fields
    if (!reqDoc.containsKey("phase") || !reqDoc.containsKey("loadWatts") || !reqDoc.containsKey("durationMinutes")) {
        sendError(400, "Missing required fields: phase, loadWatts, durationMinutes");
        return;
    }

    String phaseStr = reqDoc["phase"].as<String>();
    phaseStr.toUpperCase();

    if (phaseStr.length() != 1 || (phaseStr[0] != 'A' && phaseStr[0] != 'B' && phaseStr[0] != 'C')) {
        sendError(400, "Invalid phase. Must be A, B, or C");
        return;
    }

    uint8_t phase = phaseStr[0] - 'A';
    float loadWatts = reqDoc["loadWatts"];
    float durationMinutes = reqDoc["durationMinutes"];

    if (loadWatts <= 0 || durationMinutes <= 0) {
        sendError(400, "loadWatts and durationMinutes must be positive numbers");
        return;
    }

    // Complete calibration
    if (_energyAccumulator->completeCalibration(phase, loadWatts, durationMinutes)) {
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "Energy calibration completed for phase " + phaseStr;
        doc["phase"] = phaseStr;
        doc["loadWatts"] = loadWatts;
        doc["durationMinutes"] = durationMinutes;

        // Check if still calibrating other phases
        if (_energyAccumulator->isCalibrating()) {
            uint8_t remainingMask = _energyAccumulator->getCalibratingPhases();
            doc["stillCalibrating"] = true;
            doc["remainingPhases"] = "";
            if (remainingMask & 0x01) doc["remainingPhases"] = doc["remainingPhases"].as<String>() + "A";
            if (remainingMask & 0x02) doc["remainingPhases"] = doc["remainingPhases"].as<String>() + "B";
            if (remainingMask & 0x04) doc["remainingPhases"] = doc["remainingPhases"].as<String>() + "C";
        } else {
            doc["stillCalibrating"] = false;
            doc["message"] = doc["message"].as<String>() + ". All calibrations complete.";
        }

        sendJSON(200, doc);
    } else {
        sendError(500, "Failed to complete energy calibration");
    }
}
