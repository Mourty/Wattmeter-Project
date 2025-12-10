#include "RegisterAccess.h"
#include "EnergyWebServer.h"
#include "SDCardLogger.h"
#include "SettingsManager.h"
#include "CommandParser.h"

void CommandParser::parseCommand(const String& cmdLine) {
  String cmd = cmdLine;
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  String cmdLower = cmd;
  toLowerCase(cmdLower);

  int spaceIndex = cmdLower.indexOf(' ');

  if (spaceIndex == -1) {
    // No arguments - handle single-word commands
    if (cmdLower == "help" || cmdLower == "?") {
      printHelp();
    } else if (cmdLower == "ip") {
      handleIP();
    } else if (cmdLower == "reconnect") {
      handleReconnect();
    } else if (cmdLower == "sdstatus") {
      handleSDStatus();
    } else if (cmdLower == "sdflush") {
      handleBufferFlush();
    } else if (cmdLower == "logstart") {
      handleLogStart();
    } else if (cmdLower == "logstop") {
      handleLogStop();
    } else if (cmdLower == "logstatus") {
      handleLogStatus();
    } else if (cmdLower == "settingsload") {
      handleSettingsLoad();
    } else if (cmdLower == "settingssave") {
      handleSettingsSave();
    } else if (cmdLower == "reboot") {
      handleReboot();
    } else if (cmdLower == "uptime") { 
      handleUptime();
    } else {
      printError("Unknown command. Type 'help' for available commands.");
    }
    return;
  }

  String command = cmdLower.substring(0, spaceIndex);
  String args = cmd.substring(spaceIndex + 1);
  args.trim();

  if (command == "read") {
    handleRead(args);
  } else if (command == "write") {
    int valueSpace = args.indexOf(' ');
    if (valueSpace == -1) {
      printError("Write command requires a value. Usage: write <name> <value>");
      return;
    }

    String regName = args.substring(0, valueSpace);
    String valueStr = args.substring(valueSpace + 1);
    regName.trim();
    valueStr.trim();

    handleWrite(regName, valueStr);
  } else if (command == "wifi") {
    handleWiFi(args);
  } else if (command == "loginterval") {
    handleLogInterval(args);
  } else {
    printError("Unknown command. Type 'help' for available commands.");
  }
}

void CommandParser::handleRead(const String& regName) {
  const RegisterDescriptor* reg = _regAccess.getRegisterInfo(regName.c_str());

  if (!reg) {
    Serial.print("Error: Register '");
    Serial.print(regName);
    Serial.println("' not found.");
    return;
  }

  // Check if register is readable
  if (reg->rwType == RW_WRITE) {
    Serial.print("Error: Register '");
    Serial.print(regName);
    Serial.println("' is write-only.");
    return;
  }

  // Read raw value once (important for read-clear registers)
  bool success = false;
  uint32_t rawValue = _regAccess.readRegisterRaw(regName.c_str(), &success);

  if (!success) {
    Serial.println("Error: Failed to read register.");
    return;
  }

  // Convert the raw value
  float value = _regAccess.convertRegisterValue(regName.c_str(), rawValue);

  printRegisterValue(regName.c_str(), reg, value, rawValue);
}

void CommandParser::handleWrite(const String& regName, const String& valueStr) {
  const RegisterDescriptor* reg = _regAccess.getRegisterInfo(regName.c_str());

  if (!reg) {
    Serial.print("Error: Register '");
    Serial.print(regName);
    Serial.println("' not found.");
    return;
  }

  // Check if register is writable
  if (reg->rwType == RW_READ) {
    Serial.print("Error: Register '");
    Serial.print(regName);
    Serial.println("' is read-only.");
    return;
  }

  bool parseSuccess = false;
  uint32_t rawValue = parseValue(valueStr, &parseSuccess);

  if (!parseSuccess) {
    Serial.print("Error: Invalid value '");
    Serial.print(valueStr);
    Serial.println("'. Use decimal, hex (0x...), or binary (0b...).");
    return;
  }

  // For BIT and BITFIELD types, write raw value
  // For other types, convert from float if needed
  bool writeSuccess = false;

  if (reg->regType == DT_BIT || reg->regType == DT_BITFIELD) {
    // Write raw value for bit/bitfield
    writeSuccess = _regAccess.writeRegisterRaw(regName.c_str(), rawValue);
  } else {
    // Check if value looks like it's already scaled (contains decimal point or is hex/binary)
    if (valueStr.indexOf('.') != -1 || valueStr.startsWith("0x") || valueStr.startsWith("0b")) {
      // Treat as raw or float value
      if (valueStr.indexOf('.') != -1) {
        // Float value - use scaled write
        float floatVal = valueStr.toFloat();
        writeSuccess = _regAccess.writeRegister(regName.c_str(), floatVal);
      } else {
        // Hex or binary - use raw write
        writeSuccess = _regAccess.writeRegisterRaw(regName.c_str(), rawValue);
      }
    } else {
      // Plain decimal - treat as float for scaled write
      float floatVal = valueStr.toFloat();
      writeSuccess = _regAccess.writeRegister(regName.c_str(), floatVal);
    }
  }

  if (writeSuccess) {
    Serial.print("Success: Wrote ");
    Serial.print(valueStr);
    Serial.print(" to ");
    Serial.println(regName);

    // Read back and display
    Serial.print("Read back: ");
    bool readSuccess = false;
    float readVal = _regAccess.readRegister(regName.c_str(), &readSuccess);
    uint32_t readRaw = _regAccess.readRegisterRaw(regName.c_str(), &readSuccess);
    if (readSuccess) {
      printRegisterValue(regName.c_str(), reg, readVal, readRaw);
    }
  } else {
    Serial.println("Error: Failed to write register.");
  }
}

void CommandParser::printRegisterValue(const char* regName, const RegisterDescriptor* reg, float value, uint32_t rawValue) {
  Serial.print(reg->friendlyName);
  Serial.print(" (");
  Serial.print(regName);
  Serial.print("): ");

  // For BIT and BITFIELD types, show binary and hex
  if (reg->regType == DT_BIT) {
    Serial.print(rawValue ? "1" : "0");
    Serial.print(" (0b");
    Serial.print(rawValue, BIN);
    Serial.print(", 0x");
    Serial.print(rawValue, HEX);
    Serial.println(")");
  } else if (reg->regType == DT_BITFIELD) {
    Serial.print("0b");
    // Print with leading zeros based on bitfield length
    for (int i = reg->bitLen - 1; i >= 0; i--) {
      Serial.print((rawValue >> i) & 1);
    }
    Serial.print(" (");
    Serial.print(rawValue);
    Serial.print(", 0x");
    Serial.print(rawValue, HEX);
    Serial.println(")");
  } else {
    // For all other types, show the scaled float value
    Serial.print(value, 4);  // 4 decimal places

    // Add unit if available
    if (reg->unit && strlen(reg->unit) > 0) {
      Serial.print(" ");
      Serial.print(reg->unit);
    }

    // Show raw value in parentheses for reference
    Serial.print(" (raw: 0x");
    if (reg->regCount == 2) {
      // 32-bit value
      if (rawValue < 0x10000000) Serial.print("0");
      if (rawValue < 0x1000000) Serial.print("0");
      if (rawValue < 0x100000) Serial.print("0");
      if (rawValue < 0x10000) Serial.print("0");
      if (rawValue < 0x1000) Serial.print("0");
      if (rawValue < 0x100) Serial.print("0");
      if (rawValue < 0x10) Serial.print("0");
    } else {
      // 16-bit value
      if (rawValue < 0x1000) Serial.print("0");
      if (rawValue < 0x100) Serial.print("0");
      if (rawValue < 0x10) Serial.print("0");
    }
    Serial.print(rawValue, HEX);
    Serial.println(")");
  }
}

uint32_t CommandParser::parseValue(const String& valueStr, bool* success) {
  *success = false;

  String val = valueStr;
  val.trim();

  if (val.length() == 0) {
    return 0;
  }

  // Check for binary (0b...)
  if (val.startsWith("0b") || val.startsWith("0B")) {
    String binStr = val.substring(2);
    uint32_t result = 0;

    for (unsigned int i = 0; i < binStr.length(); i++) {
      char c = binStr.charAt(i);
      if (c == '0' || c == '1') {
        result = (result << 1) | (c - '0');
      } else if (c != '_') {  // Allow underscores for readability
        return 0;             // Invalid binary digit
      }
    }

    *success = true;
    return result;
  }

  // Check for hex (0x...)
  if (val.startsWith("0x") || val.startsWith("0X")) {
    String hexStr = val.substring(2);
    uint32_t result = 0;

    for (unsigned int i = 0; i < hexStr.length(); i++) {
      char c = hexStr.charAt(i);
      result <<= 4;

      if (c >= '0' && c <= '9') {
        result |= (c - '0');
      } else if (c >= 'a' && c <= 'f') {
        result |= (c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        result |= (c - 'A' + 10);
      } else if (c != '_') {  // Allow underscores
        return 0;             // Invalid hex digit
      }
    }

    *success = true;
    return result;
  }

  // Decimal or float - convert to appropriate type
  if (val.indexOf('.') != -1) {
    // Contains decimal point - it's a float
    *success = true;
    return (uint32_t)val.toFloat(); \
  } else {
    // Integer
    *success = true;
    return (uint32_t)val.toInt();
  }
}

void CommandParser::toLowerCase(String& str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    str.setCharAt(i, tolower(str.charAt(i)));
  }
}
void CommandParser::handleIP() {
  if (!_webServer) {
    Serial.println("Error: Web server not initialized");
    return;
  }

  String ip = _webServer->getIPAddress();
  if (ip == "0.0.0.0") {
    Serial.println("Not connected to WiFi");
  } else {
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.print("Access web interface at: http://");
    Serial.println(ip);
  }
}

void CommandParser::handleWiFi(const String& args) {
  if (!_webServer) {
    Serial.println("Error: Web server not initialized");
    return;
  }

  int spaceIndex = args.indexOf(' ');
  if (spaceIndex == -1) {
    printError("WiFi command requires SSID and password. Usage: wifi <ssid> <password>");
    return;
  }

  String ssid = args.substring(0, spaceIndex);
  String password = args.substring(spaceIndex + 1);
  ssid.trim();
  password.trim();

  if (ssid.length() == 0) {
    printError("SSID cannot be empty");
    return;
  }

  // Create WiFi settings struct
  WiFiSettings wifi;
  wifi.ssid = ssid;
  wifi.password = password;
  
  // Save to settings
  if (_settings) {
    _settings->setWiFiSettings(wifi);
    _settings->saveSettings();
  }

  Serial.print("Connecting to WiFi network: ");
  Serial.println(ssid);

  if (_webServer->begin(ssid.c_str(), password.c_str())) {
    Serial.println("Successfully connected!");
    handleIP();
  } else {
    Serial.println("Failed to connect to WiFi");
  }
}

void CommandParser::handleReconnect() {
  if (!_webServer) {
    Serial.println("Error: Web server not initialized");
    return;
  }

  Serial.println("Attempting to reconnect to WiFi...");
  if (_webServer->reconnect()) {
    Serial.println("Successfully reconnected!");
    handleIP();
  } else {
    Serial.println("Failed to reconnect to WiFi");
  }
}

void CommandParser::printError(const char* message) {
  Serial.print("Error: ");
  Serial.println(message);
}


// SD Card command handlers
void CommandParser::handleSDStatus() {
    if (!_sdLogger) {
        Serial.println("Error: SD logger not initialized");
        return;
    }
    
    Serial.println("\n=== SD Card Status ===");
    
    if (_sdLogger->isPowerLost()) {
        Serial.println("*** POWER LOST - WAITING FOR RESTORATION ***");
    }
    
    if (_sdLogger->isCardPresent()) {
        Serial.println("Card: Present");
        
        if (_sdLogger->isWriteProtected()) {
            Serial.println("Write Protection: ENABLED");
        } else {
            Serial.println("Write Protection: Disabled");
        }
        
        Serial.print("Logging: ");
        Serial.println(_sdLogger->isLoggingEnabled() ? "Enabled" : "Disabled");
        
        Serial.print("Power Status: ");
        Serial.println(_sdLogger->isPowerLost() ? "LOST (emergency mode)" : "OK");
        
        if (!_sdLogger->isPowerLost()) {
            Serial.print("Buffer: ");
            Serial.print(_sdLogger->getBufferUsage());
            Serial.print(" / ");
            Serial.print(_sdLogger->getBufferSize());
            Serial.printf(" (%.1f%% full)\n", 
                          100.0 * _sdLogger->getBufferUsage() / _sdLogger->getBufferSize());
            
            Serial.print("Total logs: ");
            Serial.println(_sdLogger->getLogCount());
        }
    } else {
        Serial.println("Card: Not present");
    }
    
    Serial.println("======================\n");
}

void CommandParser::handleBufferFlush() {
  if (!_sdLogger) {
    Serial.println("Error: SD logger not initialized");
    return;
  }

  if (_sdLogger->getBufferUsage() == 0) {
    Serial.println("Buffer is empty, nothing to flush");
    return;
  }

  Serial.printf("Manually flushing %u measurements...\n", _sdLogger->getBufferUsage());
  // The flush is handled internally by the logger
}

void CommandParser::handleLogStart() {
  if (!_sdLogger) {
    Serial.println("Error: SD logger not initialized");
    return;
  }

  _sdLogger->enableLogging(true);
  Serial.println("Data logging started");
}

void CommandParser::handleLogStop() {
  if (!_sdLogger) {
    Serial.println("Error: SD logger not initialized");
    return;
  }

  _sdLogger->enableLogging(false);
  Serial.println("Data logging stopped");
}

void CommandParser::handleLogStatus() {
  handleSDStatus();
}

void CommandParser::handleLogInterval(const String& args) {
  if (!_sdLogger) {
    Serial.println("Error: SD logger not initialized");
    return;
  }

  unsigned long interval = args.toInt();
  if (interval < 1000) {
    printError("Interval must be at least 1000 ms");
    return;
  }

  _sdLogger->setLoggingInterval(interval);
  Serial.print("Logging interval set to ");
  Serial.print(interval);
  Serial.println(" ms");
}

// Settings command handlers
void CommandParser::handleSettingsLoad() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  if (_settings->loadSettings()) {
    Serial.println("Settings loaded from SD card");
  } else {
    Serial.println("Failed to load settings");
  }
}

void CommandParser::handleSettingsSave() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  if (_settings->saveSettings()) {
    Serial.println("Settings saved to SD card");
  } else {
    Serial.println("Failed to save settings");
  }
}

void CommandParser::handleCalLoad() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  // Load entire settings file which includes calibration
  if (_settings->loadSettings()) {
    Serial.println("Settings (including calibration) loaded from SD card");
    Serial.println("Use 'calApply' to apply to chip");
  } else {
    Serial.println("Failed to load settings");
  }
}

void CommandParser::handleCalSave() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  // Save entire settings file which includes calibration
  if (_settings->saveSettings()) {
    Serial.println("Settings (including calibration) saved to SD card");
  } else {
    Serial.println("Failed to save settings");
  }
}

void CommandParser::handleCalApply() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  if (_settings->applyAllRegistersToChip()) {
    Serial.println("All registers applied to ATM90E32 chip");
  } else {
    Serial.println("Failed to apply registers");
  }
}

void CommandParser::handleCalRead() {
  if (!_settings) {
    Serial.println("Error: Settings manager not initialized");
    return;
  }

  // Display current measurement calibration settings from memory
  const MeasurementCalibrationRegisters& meas = _settings->getMeasurementCalibrationRegisters();
  const CalibrationRegisters& cal = _settings->getCalibrationRegisters();

  Serial.println("\nMeasurement Calibration Registers (RMS):");
  Serial.printf("UgainA:   0x%04X\n", meas.UgainA);
  Serial.printf("IgainA:   0x%04X\n", meas.IgainA);
  Serial.printf("UoffsetA: 0x%04X\n", meas.UoffsetA);
  Serial.printf("IoffsetA: 0x%04X\n", meas.IoffsetA);
  Serial.printf("UgainB:   0x%04X\n", meas.UgainB);
  Serial.printf("IgainB:   0x%04X\n", meas.IgainB);
  Serial.printf("UoffsetB: 0x%04X\n", meas.UoffsetB);
  Serial.printf("IoffsetB: 0x%04X\n", meas.IoffsetB);
  Serial.printf("UgainC:   0x%04X\n", meas.UgainC);
  Serial.printf("IgainC:   0x%04X\n", meas.IgainC);
  Serial.printf("UoffsetC: 0x%04X\n", meas.UoffsetC);
  Serial.printf("IoffsetC: 0x%04X\n", meas.IoffsetC);

  Serial.println("\nEnergy Calibration Registers:");
  Serial.printf("PoffsetA: 0x%04X\n", cal.PoffsetA);
  Serial.printf("QoffsetA: 0x%04X\n", cal.QoffsetA);
  Serial.printf("PQGainA:  0x%04X\n", cal.PQGainA);
  Serial.printf("PhiA:     0x%04X\n", cal.PhiA);
  Serial.printf("PoffsetB: 0x%04X\n", cal.PoffsetB);
  Serial.printf("QoffsetB: 0x%04X\n", cal.QoffsetB);
  Serial.printf("PQGainB:  0x%04X\n", cal.PQGainB);
  Serial.printf("PhiB:     0x%04X\n", cal.PhiB);
  Serial.printf("PoffsetC: 0x%04X\n", cal.PoffsetC);
  Serial.printf("QoffsetC: 0x%04X\n", cal.QoffsetC);
  Serial.printf("PQGainC:  0x%04X\n", cal.PQGainC);
  Serial.printf("PhiC:     0x%04X\n", cal.PhiC);

  Serial.println("\nUse 'calSave' to save to SD card");
}

void CommandParser::handleReboot() {
    if (!_rebootManager) {
        Serial.println("Error: Reboot manager not initialized");
        Serial.println("Performing immediate reboot...");
        delay(2000);
        ESP.restart();
        return;
    }
    
    Serial.println("Manual reboot requested...");
    _rebootManager->scheduleReboot(5000);  // 5 second warning
}

void CommandParser::handleUptime() {
    if (!_rebootManager) {
        Serial.println("Error: Reboot manager not initialized");
        Serial.printf("System uptime: %lu seconds\n", millis() / 1000);
        return;
    }
    
    unsigned long uptimeSeconds = _rebootManager->getUptimeSeconds();
    unsigned long days = uptimeSeconds / 86400;
    unsigned long hours = (uptimeSeconds % 86400) / 3600;
    unsigned long minutes = (uptimeSeconds % 3600) / 60;
    unsigned long seconds = uptimeSeconds % 60;
    
    Serial.println("\n=== System Uptime ===");
    Serial.printf("Total: %lu seconds\n", uptimeSeconds);
    Serial.printf("Time: %lu days, %lu hours, %lu minutes, %lu seconds\n", 
                  days, hours, minutes, seconds);
    
    unsigned long timeSinceReboot = _rebootManager->getTimeSinceLastReboot();
    if (timeSinceReboot > 0) {
        unsigned long rebootDays = timeSinceReboot / 86400;
        unsigned long rebootHours = (timeSinceReboot % 86400) / 3600;
        Serial.printf("Time since last reboot: %lu days, %lu hours\n", 
                      rebootDays, rebootHours);
    }
    
    Serial.println("====================\n");
}


void CommandParser::printHelp() {
  Serial.println("\n=== ATM90E32 Energy Monitor Commands ===");
  Serial.println("\n--- Register Access ---");
  Serial.println("  read <name>            - Read a register by name");
  Serial.println("  write <name> <val>     - Write a value to a register");

  Serial.println("\n--- Network ---");
  Serial.println("  ip                     - Show current IP address");
  Serial.println("  wifi <ssid> <password> - Connect to WiFi network");
  Serial.println("  reconnect              - Reconnect to last WiFi");

  Serial.println("\n--- SD Card & Data Logging ---");
  Serial.println("  sdstatus               - Show SD card status");
  Serial.println("  logstart               - Start data logging");
  Serial.println("  logstop                - Stop data logging");
  Serial.println("  logstatus              - Show logging status");
  Serial.println("  loginterval <ms>       - Set logging interval (milliseconds)");
  Serial.println("  sdflush                - Manually flush buffer to SD card");

  Serial.println("\n--- Settings & Calibration ---");
  Serial.println("  settingsload           - Load settings from SD card");
  Serial.println("  settingssave           - Save settings to SD card");
  Serial.println("  calload                - Load calibration from SD card");
  Serial.println("  calsave                - Save calibration to SD card");
  Serial.println("  calapply               - Apply calibration to chip");
  Serial.println("  calread                - Read calibration from chip");

  Serial.println("\n--- System ---");
  Serial.println("  reboot                 - Reboot system (5 second delay)");
  Serial.println("  uptime                 - Show system uptime");

  Serial.println("\n--- General ---");
  Serial.println("  help or ?              - Show this help");

  Serial.println("\nValue Formats:");
  Serial.println("  Decimal:  123 or 123.45");
  Serial.println("  Hex:      0x1A2B");
  Serial.println("  Binary:   0b10110101");

  Serial.println("\nExamples:");
  Serial.println("  read UrmsA                  - Read voltage");
  Serial.println("  write MeterEn 1             - Enable meter");
  Serial.println("  wifi MyNetwork MyPass       - Connect to WiFi");
  Serial.println("  logstart                    - Start logging");
  Serial.println("  reboot                      - Restart system");
  Serial.println("==========================================\n");
}