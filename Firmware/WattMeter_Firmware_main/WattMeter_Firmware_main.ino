#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <Timezone.h>
#include <Time.h>
#include "ATM90E32.h"
#include "RegisterAccess.h"
#include "CommandParser.h"
#include "EnergyWebServer.h"
#include "SDCardLogger.h"
#include "SettingsManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"
#include "RebootManager.h"
#include "EnergyAccumulator.h"



// Pin definitions
#define PIN_SPI_M90E32_CS 2  // Chip Select for M90ES32  (Active low)
#define SD_CS_PIN 20         // Chip Select for SD card  (Active Low)
#define SD_CD_PIN 10         // Card Detect
#define SD_WP_PIN 11         // Write Protect
#define PIN_SPI_SCLK 18      // Clock
#define PIN_SPI_MISO 19      // Master In Slave Out
#define PIN_SPI_MOSI 23      // Master Out Slave In
#define PIN_CAP_SENSE 1      // Super cap voltage sense pin
#define I2C_SDA 21           // I2C SDA pin
#define I2C_SDL 22           // I2C SDL pin
#define BUTTON_PIN 3


enum FaultCode {
  FAULT_NONE = 0,
  FAULT_SD_MISSING,
  FAULT_SD_SETTINGS_MISSING,
  FAULT_CHIP_INIT_FAILED,
  FAULT_RTC_INIT_FAILED
};

enum WarningCode {
  WARNING_NONE = 0,
  WARNING_SD_WRITE_PROTECTED
};



//Global Objects
LiquidCrystal_I2C lcd(0x27, 20, 4);
ATM90E32 energyChip(PIN_SPI_M90E32_CS);
RegisterAccess regAccess(energyChip);
TimeManager timeManager;
SDCardLogger sdLogger(regAccess, timeManager, SD_CS_PIN, SD_CD_PIN, SD_WP_PIN);
CommandParser cmdParser(regAccess);
EnergyWebServer EnergyWebServer(regAccess);
SettingsManager settings(regAccess);
EnergyAccumulator energyAccumulator(regAccess);
DisplayManager displayManager(regAccess, timeManager, sdLogger, EnergyWebServer, BUTTON_PIN);
RebootManager rebootManager(timeManager, sdLogger);



// Serial command buffer
String serialCommand = "";
//Heap check timer
static unsigned long lastHeapCheck = 0;

// ================ Settings Helpers ======================

// Apply WiFi settings to web server
bool applyWiFiSettings(const WiFiSettings& wifi) {
  if (wifi.ssid.length() == 0) {
    Serial.println("No WiFi SSID configured");
    return false;
  }

  Serial.println("\nConnecting to WiFi from settings...");
  if (EnergyWebServer.begin(wifi.ssid.c_str(), wifi.password.c_str())) {
    Serial.println("Connected to WiFi from saved settings");
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}

// Apply RTC calibration settings and timezone
void applyRTCSettings(const RTCCalibrationSettings& rtc, const TimezoneSettings& tz) {
  Serial.println("Applying RTC calibration settings...");
  timeManager.setMinCalibrationDays(rtc.minCalibrationDays);
  timeManager.setCalibrationThreshold(rtc.calibrationThreshold);
  timeManager.setCalibrationEnabled(rtc.autoCalibrationEnabled);
  timeManager.setCalibrationData(rtc.lastCalibrationTime, rtc.currentOffset);

  if (rtc.currentOffset != 0) {
    timeManager.applyCalibrationOffset(rtc.currentOffset);
  }

  // Configure timezone
  char dstAbbrevBuffer[6];
  char stdAbbrevBuffer[6];

  // Copy the abbreviations into static storage
  strncpy(dstAbbrevBuffer, tz.dstAbbrev.c_str(), 5);
  dstAbbrevBuffer[5] = '\0';
  strncpy(stdAbbrevBuffer, tz.stdAbbrev.c_str(), 5);
  stdAbbrevBuffer[5] = '\0';


  TimeChangeRule dstRule = {
    "DST",
    tz.dstWeek,
    tz.dstDow,
    tz.dstMonth,
    tz.dstHour,
    tz.dstOffset
  };

  TimeChangeRule stdRule = {
    "STD",
    tz.stdWeek,
    tz.stdDow,
    tz.stdMonth,
    tz.stdHour,
    tz.stdOffset
  };

  timeManager.setTimezone(dstRule, stdRule);
}


// Sync RTC with NTP if needed
bool syncRTCIfNeeded(const RTCCalibrationSettings& rtc) {
  if (!timeManager.isRTCValid() || !timeManager.hasBeenSynced()) {
    Serial.println("\nSyncing RTC with NTP...");

    if (timeManager.syncFromNTP(rtc.ntpServer.c_str())) {
      RTCCalibrationSettings newSettings = rtc;
      time_t refTime;
      int8_t offset;
      timeManager.getCalibrationData(refTime, offset);
      newSettings.lastCalibrationTime = refTime;
      newSettings.currentOffset = offset;
      settings.setRTCCalibration(newSettings);
      timeManager.setAutoSyncInterval(86400);  // 24 hours
      settings.saveSettings();

      return true;
    }

    return false;
  } else {
    Serial.println("\nRTC already has valid time, skipping NTP sync");
    Serial.print("Current time: ");
    Serial.println(timeManager.getTimeString());

    // Set auto-sync for once per day
    timeManager.setAutoSyncInterval(86400);  // 24 hours
    return true;
  }
}

// Apply data logging settings
void applyDataLoggingSettings(const DataLoggingSettings& log) {
  Serial.println("Applying data logging settings...");

  // Set log fields FIRST (before allocating buffer)
  sdLogger.setLogFields(log.logFields);

  sdLogger.setBufferSize(log.bufferSize);
  sdLogger.setLoggingInterval(log.loggingInterval);
  sdLogger.setPowerLossThreshold(log.powerLossThreshold);
  sdLogger.enablePowerLossDetection(log.enablePowerLossDetection);
}

// Apply display settings
void applyDisplaySettings(const DisplaySettings& disp) {
  Serial.println("Applying display settings...");
  displayManager.setDisplayFields(disp.field0, disp.field1, disp.field2);
  displayManager.setBacklightTimeout(disp.backlightTimeout);
  displayManager.setLongPressTime(disp.longPressTime);
}

// Apply system settings
void applySystemSettings(const SystemSettings& sys) {
  Serial.println("Applying system settings...");
  rebootManager.enable(sys.autoRebootEnabled);
  rebootManager.setRebootInterval(sys.rebootIntervalHours);
  rebootManager.setRebootHour(sys.rebootHour);
}

// Apply all settings (for use after loading or reloading)
void applyAllSettings() {
  Serial.println("\n=== Applying All Settings ===");

  // Apply all registers to ATM90E32
  Serial.println("Applying all registers to ATM90E32...");
  settings.applyAllRegistersToChip();

  // Apply RTC settings AND timezone together
  applyRTCSettings(settings.getRTCCalibration(), settings.getTimezoneSettings());
  // Apply data logging settings
  applyDataLoggingSettings(settings.getDataLoggingSettings());
  // Apply display settings
  applyDisplaySettings(settings.getDisplaySettings());
  // Apply system settings
  applySystemSettings(settings.getSystemSettings());

  // Apply WiFi and sync time if possible
  if (applyWiFiSettings(settings.getWiFiSettings())) {
    syncRTCIfNeeded(settings.getRTCCalibration());

    // Enable logging if RTC is valid
    if (timeManager.isRTCValid()) {
      sdLogger.enableLogging(true);
    }
  }

  Serial.println("=== All Settings Applied ===\n");
}

void applyAllButWIFISettings() {
  Serial.println("\n=== Applying All Settings But WIFI ===");

  // Apply all registers to ATM90E32
  Serial.println("Applying all registers to ATM90E32...");
  settings.applyAllRegistersToChip();

  // Apply RTC settings AND timezone together
  applyRTCSettings(settings.getRTCCalibration(), settings.getTimezoneSettings());
  // Apply data logging settings
  applyDataLoggingSettings(settings.getDataLoggingSettings());
  // Apply display settings
  applyDisplaySettings(settings.getDisplaySettings());
  // Apply system settings
  applySystemSettings(settings.getSystemSettings());

  Serial.println("=== All Settings Applied But WIFI ===\n");
}


// ============== Fault and Warning System ==============





// Global warning state
struct {
  WarningCode activeWarning;
  unsigned long lastWarningDisplay;
  unsigned long warningDisplayInterval;
  unsigned long warningDisplayDuration;
  bool warningDisplayed;
} warningState = { WARNING_NONE, 0, 5000, 3000, false };

// Display a critical fault and halt system
void displayFault(FaultCode fault) {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("*** FAULT ***");

  const char* line1 = nullptr;
  const char* line2 = nullptr;

  switch (fault) {
    case FAULT_SD_MISSING:
      line1 = "NO SD CARD FOUND";
      line2 = "Insert SD card";
      break;

    case FAULT_SD_SETTINGS_MISSING:
      line1 = "Settings file";
      line2 = "missing or invalid";
      break;

    case FAULT_CHIP_INIT_FAILED:
      line1 = "ATM90E32 chip";
      line2 = "init failed";
      break;

    case FAULT_RTC_INIT_FAILED:
      line1 = "RTC initialization";
      line2 = "failed";
      break;

    default:
      line1 = "Unknown fault";
      break;
  }

  if (line1) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }

  if (line2) {
    lcd.setCursor(0, 2);
    lcd.print(line2);
  }

  // Log to serial
  Serial.println("\n=============================");
  Serial.println("*** CRITICAL FAULT ***");
  if (line1) Serial.println(line1);
  if (line2) Serial.println(line2);
  Serial.println("System will retry in 30 seconds...");
  Serial.println("=============================\n");

  // Display countdown and wait before retry
  for (int countdown = 30; countdown > 0; countdown--) {
    lcd.setCursor(0, 3);
    lcd.print("Retry in ");
    lcd.print(countdown);
    lcd.print(" sec   ");  // Extra spaces to clear previous longer numbers

    Serial.print("Retrying in ");
    Serial.print(countdown);
    Serial.println(" seconds...");

    delay(1000);
  }

  // Clear screen and show rebooting message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Rebooting...");
  Serial.println("Rebooting now...");
  delay(1000);

  // Restart the ESP32
  ESP.restart();
}

// Set an active warning (non-blocking)
void setWarning(WarningCode warning) {
  if (warningState.activeWarning != warning) {
    warningState.activeWarning = warning;
    warningState.warningDisplayed = false;
    warningState.lastWarningDisplay = 0;  // Display immediately
    Serial.print("WARNING: ");
    Serial.println(getWarningString(warning));
  }
}

// Clear active warning
void clearWarning() {
  if (warningState.activeWarning != WARNING_NONE) {
    Serial.println("Warning cleared");
    warningState.activeWarning = WARNING_NONE;
    warningState.warningDisplayed = false;
    // Force display update to restore normal view
    displayManager.forceUpdate();
  }
}

// Get warning string for serial output
const char* getWarningString(WarningCode warning) {
  switch (warning) {
    case WARNING_SD_WRITE_PROTECTED:
      return "SD card write protected";
    default:
      return "Unknown warning";
  }
}

// Display warning temporarily on LCD
void displayWarningOnLCD(WarningCode warning) {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("!!! WARNING !!!");

  const char* line1 = nullptr;
  const char* line2 = nullptr;
  const char* line3 = nullptr;

  switch (warning) {
    case WARNING_SD_WRITE_PROTECTED:
      line1 = "SD card is";
      line2 = "write protected";
      line3 = "Logging disabled";
      break;

    default:
      line1 = "Unknown warning";
      break;
  }

  if (line1) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
  }

  if (line2) {
    lcd.setCursor(0, 2);
    lcd.print(line2);
  }

  if (line3) {
    lcd.setCursor(0, 3);
    lcd.print(line3);
  }
}

// Update warning display system
void updateWarningDisplay() {
  unsigned long now = millis();

  // No active warning
  if (warningState.activeWarning == WARNING_NONE) {
    warningState.warningDisplayed = false;
    return;
  }

  // Check if it's time to display warning
  if (!warningState.warningDisplayed && (now - warningState.lastWarningDisplay >= warningState.warningDisplayInterval)) {

    // Display the warning
    displayWarningOnLCD(warningState.activeWarning);
    warningState.warningDisplayed = true;
    warningState.lastWarningDisplay = now;
  }

  // Check if it's time to restore normal display
  if (warningState.warningDisplayed && (now - warningState.lastWarningDisplay >= warningState.warningDisplayDuration)) {

    // Restore normal display
    displayManager.forceUpdate();
    warningState.warningDisplayed = false;
  }
}

// Configure warning display timing
void setWarningTiming(unsigned long displayInterval, unsigned long displayDuration) {
  warningState.warningDisplayInterval = displayInterval;
  warningState.warningDisplayDuration = displayDuration;
}


// ================ Setup ======================


void setup() {
  Wire.begin(I2C_SDA, I2C_SDL);

  Serial.begin(115200);

  Serial.println("\n\n=== ATM90E32 Energy Monitor ===");

  // Initialize LCD early so we can display errors
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Energy Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Initialize hardware
  SPI.begin(PIN_SPI_SCLK, PIN_SPI_MISO, PIN_SPI_MOSI);

  if (!energyChip.begin()) {
    Serial.println("ERROR: Failed to initialize ATM90E32");
    displayFault(FAULT_CHIP_INIT_FAILED);
  }
  Serial.println("ATM90E32 initialized");

  if (!timeManager.begin()) {
    Serial.println("ERROR: Failed to initialize RTC");
    displayFault(FAULT_RTC_INIT_FAILED);
  }

  // Critical: SD card must be present with settings
  if (!sdLogger.begin()) {
    Serial.println("ERROR: SD Card initialization failed");
    displayFault(FAULT_SD_MISSING);
  }

  // SD card is present, but can we load settings?
  if (!settings.loadSettings()) {
    Serial.println("ERROR: Failed to load settings from SD card");
    displayFault(FAULT_SD_SETTINGS_MISSING);
  }

  Serial.println("Settings loaded from SD card");

  // Continue with normal initialization
  displayManager.begin();

  // Apply all loaded settings
  applyAllSettings();

  // Link components to command parser
  cmdParser.setWebServer(&EnergyWebServer);
  cmdParser.setSDLogger(&sdLogger);
  cmdParser.setSettingsManager(&settings);
  cmdParser.setRebootManager(&rebootManager);

  //link settings manager to the webserver
  EnergyWebServer.setSettingsManager(&settings);

  // Initialize energy accumulator
  energyAccumulator.begin(&settings);
  EnergyWebServer.setEnergyAccumulator(&energyAccumulator);
  sdLogger.setEnergyAccumulator(&energyAccumulator);

  // Initialize reboot manager
  rebootManager.begin();

  // Configure warning display (5 seconds between, 3 seconds duration)
  setWarningTiming(5000, 3000);

  Serial.println("\n=== System Ready ===");
  Serial.print("Current time: ");
  Serial.println(timeManager.getTimeString());
  Serial.println("Type 'help' for available commands\n");
  cmdParser.printHelp();
  Serial.print("> ");
}

// ================ Main Loop ======================

void loop() {
  // Update warning display system
  updateWarningDisplay();

  // Handle web server requests
  EnergyWebServer.handleClient();

  // Update TimeManager (handles auto-sync with NTP)
  timeManager.update();

  // Handle SD card logging
  sdLogger.update();

  // Update energy accumulator (reads energy registers, saves periodically)
  energyAccumulator.update();

  // Update display (handles button, backlight, refresh)
  // Only update if no warning is currently being displayed
  if (!warningState.warningDisplayed) {
    displayManager.update();
  }


  // Check for warning condition(s)
  if (sdLogger.isCardPresent() && sdLogger.isWriteProtected()) {
    setWarning(WARNING_SD_WRITE_PROTECTED);
  } else {
    clearWarning();  // Clear if no warnings present
  }


  // Handle serial commands
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (serialCommand.length() > 0) {
        cmdParser.parseCommand(serialCommand);
        serialCommand = "";
        Serial.print("\n> ");
      }
    } else {
      serialCommand += c;
    }
  }

  if (EnergyWebServer.settingsNeedReload()) {
    Serial.println("\n=== Reloading Settings After Remote Change ===");
    applyAllButWIFISettings();
  }

  if (sdLogger.settingsNeedReload()) {
    Serial.println("\n=== Reloading Settings After Power Restoration ===");

    if (settings.loadSettings()) {
      applyAllSettings();
      Serial.println("All settings reloaded and applied");
    } else {
      Serial.println("Failed to reload settings");
    }
  }


  if (millis() - lastHeapCheck >= 60000) {
    Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
    lastHeapCheck = millis();
  }

  // Small delay to prevent watchdog issues
  rebootManager.update();
  delay(1);
}
