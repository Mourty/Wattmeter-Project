#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "RegisterAccess.h"
#include "TimeManager.h"
#include "SDCardLogger.h"
#include "EnergyWebServer.h"
#include <Wire.h>

class DisplayManager {
public:
    DisplayManager(RegisterAccess& regAccess, TimeManager& timeManager, 
                   SDCardLogger& sdLogger, EnergyWebServer& webServer,
                   int buttonPin, uint8_t lcdAddress = 0x27);
    
    bool begin();
    void update();
    
    // Configuration
    void setDisplayFields(const String& line0, const String& line1, const String& line2);
    void setBacklightTimeout(unsigned long timeoutMs);
    void setLongPressTime(unsigned long pressMs);
    
    // Manual control
    void turnOnBacklight();
    void turnOffBacklight();
    void forceUpdate();
    
private:
    RegisterAccess& _regAccess;
    TimeManager& _timeManager;
    SDCardLogger& _sdLogger;
    EnergyWebServer& _webServer;
    
    LiquidCrystal_I2C _lcd;
    int _buttonPin;
    
    // Display fields (register names)
    String _field0;
    String _field1;
    String _field2;
    
    // Backlight control
    bool _backlightOn;
    unsigned long _backlightTimeout;
    unsigned long _backlightOnTime;
    
    // Button handling
    bool _lastButtonState;
    bool _buttonPressed;
    unsigned long _buttonPressStartTime;
    unsigned long _longPressTime;
    bool _longPressHandled;
    
    // Update timing
    unsigned long _lastDisplayUpdate;
    static const unsigned long DISPLAY_UPDATE_INTERVAL = 500;  // Update every 500ms
    
    // Helper methods
    void updateDisplay();
    void updateButton();
    void handleShortPress();
    void handleLongPress();
    void checkBacklightTimeout();
    
    String formatValue(const char* fieldName, float value);
    byte getWiFiIcon();
    byte getSDIcon();
    
    // Custom characters for icons
    void createCustomChars();
};

#endif