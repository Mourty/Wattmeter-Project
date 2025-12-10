#include "Arduino.h"
#include <stdio.h>
#include "DisplayManager.h"

// Custom character definitions (5x8 pixels)
// WiFi icon
byte wifiChar[8] = {
  0b00000,
  0b01110,
  0b10001,
  0b00100,
  0b01010,
  0b00000,
  0b00100,
  0b00000
};

// SD card present icon
byte sdPresentChar[8] = {
  0b00111,
  0b00101,
  0b00111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

// SD card writing icon (animated with arrows)
byte sdWritingChar[8] = {
  0b00111,
  0b00101,
  0b00111,
  0b11011,
  0b10101,
  0b11111,
  0b10101,
  0b11011
};

// SD card unmounted icon
byte sdUnmountedChar[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};

// No SD card icon
byte sdNoneChar[8] = {
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

DisplayManager::DisplayManager(RegisterAccess& regAccess, TimeManager& timeManager,
                               SDCardLogger& sdLogger, EnergyWebServer& webServer,
                               int buttonPin, uint8_t lcdAddress)
  : _regAccess(regAccess),
    _timeManager(timeManager),
    _sdLogger(sdLogger),
    _webServer(webServer),
    _lcd(lcdAddress, 20, 4),  // 20x4 LCD
    _buttonPin(buttonPin),
    _field0("UrmsA"),
    _field1("IrmsA"),
    _field2("PmeanA"),
    _backlightOn(true),
    _backlightTimeout(30000),  // 30 seconds default
    _backlightOnTime(0),
    _lastButtonState(HIGH),
    _buttonPressed(false),
    _buttonPressStartTime(0),
    _longPressTime(10000),  // 10 seconds for long press
    _longPressHandled(false),
    _lastDisplayUpdate(0) {
}

bool DisplayManager::begin() {
  // Setup button pin
  pinMode(_buttonPin, INPUT_PULLUP);

  // Initialize LCD
  _lcd.init();
  _lcd.backlight();
  _backlightOn = true;
  _backlightOnTime = millis();

  // Create custom characters
  createCustomChars();


  // Display startup message
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("Energy Monitor");
  _lcd.setCursor(0, 1);
  _lcd.print("Initializing...");

  delay(2000);
  _lcd.clear();

  // Initial display update
  updateDisplay();

  Serial.println("Display Manager initialized");

  return true;
}

void DisplayManager::createCustomChars() {
  _lcd.createChar(0, wifiChar);
  _lcd.createChar(1, sdPresentChar);
  _lcd.createChar(2, sdWritingChar);
  _lcd.createChar(3, sdUnmountedChar);
  _lcd.createChar(4, sdNoneChar);
}

void DisplayManager::setDisplayFields(const String& line0, const String& line1, const String& line2) {
  _field0 = line0;
  _field1 = line1;
  _field2 = line2;

  Serial.print("Display fields set: ");
  Serial.print(_field0);
  Serial.print(", ");
  Serial.print(_field1);
  Serial.print(", ");
  Serial.println(_field2);

  forceUpdate();
}

void DisplayManager::setBacklightTimeout(unsigned long timeoutMs) {
  _backlightTimeout = timeoutMs;
}

void DisplayManager::setLongPressTime(unsigned long pressMs) {
  _longPressTime = pressMs;
}

void DisplayManager::update() {
  unsigned long now = millis();

  // Update button state
  updateButton();

  // Check backlight timeout
  checkBacklightTimeout();

  // Update display periodically
  if (now - _lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    _lastDisplayUpdate = now;
  }
}

void DisplayManager::updateButton() {
  bool currentState = digitalRead(_buttonPin);

  // Detect button press (HIGH to LOW transition)
  if (currentState == LOW && _lastButtonState == HIGH) {
    _buttonPressed = true;
    _buttonPressStartTime = millis();
    _longPressHandled = false;
  }

  // Button is being held
  if (currentState == LOW && _buttonPressed) {
    unsigned long pressDuration = millis() - _buttonPressStartTime;

    // Check for long press
    if (pressDuration >= _longPressTime && !_longPressHandled) {
      handleLongPress();
      _longPressHandled = true;
    }
  }

  // Detect button release
  if (currentState == HIGH && _lastButtonState == LOW) {
    if (_buttonPressed && !_longPressHandled) {
      // Short press
      unsigned long pressDuration = millis() - _buttonPressStartTime;
      if (pressDuration < _longPressTime) {
        handleShortPress();
      }
    }
    _buttonPressed = false;
  }

  _lastButtonState = currentState;
}

void DisplayManager::handleShortPress() {
  Serial.println("Button: Short press - turning on backlight");
  turnOnBacklight();
}

void DisplayManager::handleLongPress() {
  Serial.println("Button: Long press - safe SD card removal");

  // Display message
  _lcd.clear();
  _lcd.setCursor(0, 0);
  _lcd.print("Flushing buffer...");

  // Flush buffer and unmount SD card
  if (_sdLogger.isCardPresent()) {
    _sdLogger.handlePowerLoss();

    _lcd.setCursor(0, 1);
    _lcd.print("SD card safe to");
    _lcd.setCursor(0, 2);
    _lcd.print("remove");
  } else {
    _lcd.setCursor(0, 1);
    _lcd.print("No SD card present");
  }

  delay(3000);
  updateDisplay();
}

void DisplayManager::turnOnBacklight() {
  if (!_backlightOn) {
    _lcd.backlight();
    _backlightOn = true;
  }
  _backlightOnTime = millis();
}

void DisplayManager::turnOffBacklight() {
  if (_backlightOn) {
    _lcd.noBacklight();
    _backlightOn = false;
  }
}

void DisplayManager::checkBacklightTimeout() {
  if (_backlightOn && _backlightTimeout > 0) {
    unsigned long now = millis();
    if (now - _backlightOnTime >= _backlightTimeout) {
      turnOffBacklight();
      Serial.println("Backlight timed out");
    }
  }
}

void DisplayManager::forceUpdate() {
  _lastDisplayUpdate = 0;  // Force update on next loop
}

void DisplayManager::updateDisplay() {
  // Line 0: Field 0 + WiFi icon
  _lcd.setCursor(0, 0);
  bool success = false;
  float value0 = _regAccess.readRegister(_field0.c_str(), &success);
  
  // Get register info for units
  const RegisterDescriptor* reg0 = _regAccess.getRegisterInfo(_field0.c_str());
  const char* unit0 = (reg0 && reg0->unit) ? reg0->unit : "";
  
  char line0[20];
  snprintf(line0, sizeof(line0), "%7.3f%-4s", value0, unit0);  // Reserve 4 chars for unit
  // Pad to 18 characters total (leave room for WiFi icon)
  int len0 = strlen(line0);
  for (int i = len0; i < 18; i++) {
    line0[i] = ' ';
  }
  line0[18] = '\0';
  _lcd.print(line0);
  
  // WiFi icon at position 19
  _lcd.setCursor(19, 0);
  _lcd.write(getWiFiIcon());
  
  // Line 1: Field 1 + SD icon
  _lcd.setCursor(0, 1);
  float value1 = _regAccess.readRegister(_field1.c_str(), &success);
  
  const RegisterDescriptor* reg1 = _regAccess.getRegisterInfo(_field1.c_str());
  const char* unit1 = (reg1 && reg1->unit) ? reg1->unit : "";
  
  char line1[20];
  snprintf(line1, sizeof(line1), "%7.3f%-4s", value1, unit1);
  int len1 = strlen(line1);
  for (int i = len1; i < 18; i++) {
    line1[i] = ' ';
  }
  line1[18] = '\0';
  _lcd.print(line1);
  
  // SD icon at position 19
  _lcd.setCursor(19, 1);
  _lcd.write(getSDIcon());
  
  // Line 2: Field 2
  _lcd.setCursor(0, 2);
  float value2 = _regAccess.readRegister(_field2.c_str(), &success);
  
  const RegisterDescriptor* reg2 = _regAccess.getRegisterInfo(_field2.c_str());
  const char* unit2 = (reg2 && reg2->unit) ? reg2->unit : "";
  
  char line2[20];
  snprintf(line2, sizeof(line2), "%7.3f%-4s", value2, unit2);
  int len2 = strlen(line2);
  for (int i = len2; i < 20; i++) {
    line2[i] = ' ';
  }
  line2[20] = '\0';
  _lcd.print(line2);
  
  // Line 3: Date and Time
  _lcd.setCursor(0, 3);
  if (_timeManager.isRTCValid()) {
    char buffer[21];
    _timeManager.getLocalTimeString().toCharArray(buffer, 21);
    _lcd.print(buffer);
  } else {
    _lcd.print("Time: Not Set       ");
  }
}

String DisplayManager::formatValue(const char* fieldName, float value) {
  const RegisterDescriptor* reg = _regAccess.getRegisterInfo(fieldName);

  String result = "";

  // Get abbreviated name
  if (reg != nullptr && reg->name != nullptr) {
    result = String(reg->name);
    result += ": ";
  } else {
    result = String(fieldName);
    result += ": ";
  }

  // Format value based on register type
  char valueStr[12];
  if (reg != nullptr) {
    if (reg->regType == DT_INT16 || reg->regType == DT_INT32) {
      sprintf(valueStr, "%.0f", value);
    } else {
      // Determine decimal places
      if (String(fieldName).indexOf("rms") >= 0 && String(fieldName).startsWith("I")) {
        sprintf(valueStr, "%.3f", value);  // Current
      } else {
        sprintf(valueStr, "%.2f", value);  // Voltage, Power, etc
      }
    }
  } else {
    sprintf(valueStr, "%.2f", value);
  }

  result += String(valueStr);

  // Add unit if available
  if (reg != nullptr && reg->unit != nullptr && strlen(reg->unit) > 0) {
    result += String(reg->unit);
  }

  return result;
}

byte DisplayManager::getWiFiIcon() {
  if (_webServer.getIPAddress() != "0.0.0.0") {
    return 0;  // WiFi connected icon
  }
  return ' ';  // No WiFi
}

byte DisplayManager::getSDIcon() {
  if (_sdLogger.isPowerLost()) {
    return 3;  // Unmounted (safe removal)
  } else if (!_sdLogger.isCardPresent()) {
    return 4;  // No card
  } else if (_sdLogger.isLoggingEnabled() && _sdLogger.getBufferUsage() > (_sdLogger.getBufferSize() * 0.8)) {
    return 2;  // Writing (buffer almost full)
  } else if (_sdLogger.isCardPresent()) {
    return 1;  // Card present
  }
  return ' ';
}