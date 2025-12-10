#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "RegisterAccess.h"

// Settings structure
struct WiFiSettings {
    String ssid;
    String password;
};

struct RTCCalibrationSettings {
    time_t lastCalibrationTime;         // Unix timestamp of last calibration
    int8_t currentOffset;               // Current calibration offset applied (-64 to +63)
    bool calibrationEnabled;            // Whether auto-calibration is enabled
    unsigned long minCalibrationDays;   // minimum days between calibration
    float calibrationThreshold;         // minimum drift to change calibration
    bool autoCalibrationEnabled;        // enable or disable auto drift calibration
    String ntpServer;                   // What server to sync time with
};

struct TimezoneSettings {
    // Daylight/Summer time
    String dstAbbrev;          // "CDT", "BST", etc.
    uint8_t dstWeek;           // First, Second, Third, Fourth, or Last week of month (1-4, 5=last)
    uint8_t dstDow;            // Day of week (1=Sun, 2=Mon, ..., 7=Sat)
    uint8_t dstMonth;          // 1-12
    uint8_t dstHour;           // 0-23
    int16_t dstOffset;         // Offset from UTC in minutes
    
    // Standard time
    String stdAbbrev;          // "CST", "GMT", etc.
    uint8_t stdWeek;           // First, Second, Third, Fourth, or Last week of month
    uint8_t stdDow;            // Day of week
    uint8_t stdMonth;          // 1-12
    uint8_t stdHour;           // 0-23
    int16_t stdOffset;         // Offset from UTC in minutes
};

struct DataLoggingSettings {
    unsigned long loggingInterval;     // Milliseconds between readings
    unsigned int bufferSize;           // Number of readings to buffer (1-1000)
    float powerLossThreshold;          // Voltage threshold to detect power loss
    bool enablePowerLossDetection;     // Enable/disable power loss detection
    String logFields;                  // Comma-separated list of register names to log
};

struct DisplaySettings {
    String field0;              // Register name for line 0
    String field1;              // Register name for line 1
    String field2;              // Register name for line 2
    unsigned long backlightTimeout;  // Milliseconds (0 = always on)
    unsigned long longPressTime;     // Milliseconds for long press detection
};

struct SystemSettings {
    bool autoRebootEnabled;
    unsigned long rebootIntervalHours;  // Hours between reboots (0 = disabled)
    int rebootHour;                      // Hour of day to reboot (0-23, -1 = any time)
};

// Status and Special Registers (raw hex values)
struct StatusAndSpecialRegisters {
    uint16_t IA_SRC;
    uint16_t IB_SRC;
    uint16_t IC_SRC;
    uint16_t UA_SRC;
    uint16_t UB_SRC;
    uint16_t UC_SRC;
    uint16_t Sag_Period;
    uint16_t PeakDet_period;
    uint16_t OVth;
    uint16_t Zxdis;
    uint16_t ZX0Con;
    uint16_t ZX1Con;
    uint16_t ZX2Con;
    uint16_t ZX0Src;
    uint16_t ZX1Src;
    uint16_t ZX2Src;
    uint16_t SagTh;
    uint16_t PhaseLossTh;
    uint16_t InWarnTh;
    uint16_t OIth;
    uint16_t FreqLoTh;
    uint16_t FreqHiTh;
    uint16_t IRQ1_OR;
    uint16_t WARN_OR;
};

// Configuration Registers (raw hex values)
struct ConfigurationRegisters {
    uint32_t PL_Constant;
    uint16_t EnPC;
    uint16_t EnPB;
    uint16_t EnPA;
    uint16_t ABSEnP;
    uint16_t ABSEnQ;
    uint16_t CF2varh;
    uint16_t _3P3W;  
    uint16_t didtEn;
    uint16_t HPFoff;
    uint16_t Freq60Hz;
    uint16_t PGA_GAIN;
    uint16_t PStartTh;
    uint16_t QStartTh;
    uint16_t SStartTh;
    uint16_t PPhaseTh;
    uint16_t QPhaseTh;
    uint16_t SPhaseTh;
};

// Calibration Registers (raw hex values - energy calibration)
struct CalibrationRegisters {
    uint16_t PoffsetA;
    uint16_t QoffsetA;
    uint16_t PoffsetB;
    uint16_t QoffsetB;
    uint16_t PoffsetC;
    uint16_t QoffsetC;
    uint16_t PQGainA;
    uint16_t PhiA;
    uint16_t PQGainB;
    uint16_t PhiB;
    uint16_t PQGainC;
    uint16_t PhiC;
};

// Fundamental Harmonic Energy Calibration Registers (raw hex values)
struct FundamentalHarmonicCalibrationRegisters {
    uint16_t PoffsetAF;
    uint16_t PoffsetBF;
    uint16_t PoffsetCF;
    uint16_t PGainAF;
    uint16_t PGainBF;
    uint16_t PGainCF;
};

// Measurement Calibration Registers (raw hex values - RMS calibration)
struct MeasurementCalibrationRegisters {
    uint16_t UgainA;
    uint16_t IgainA;
    uint16_t UoffsetA;
    uint16_t IoffsetA;
    uint16_t UgainB;
    uint16_t IgainB;
    uint16_t UoffsetB;
    uint16_t IoffsetB;
    uint16_t UgainC;
    uint16_t IgainC;
    uint16_t UoffsetC;
    uint16_t IoffsetC;
};

// EMM Status Registers (raw hex values - interrupt enables)
struct EMMStatusRegisters {
    uint16_t CF4RevIntEN;
    uint16_t CF3RevIntEN;
    uint16_t CF2RevIntEN;
    uint16_t CF1RevIntEN;
    uint16_t TASNoloadIntEN;
    uint16_t TPNoloadIntEN;
    uint16_t TQNoloadIntEN;
    uint16_t INOv0IntEN;
    uint16_t IRevWnIntEN;
    uint16_t URevWnIntEN;
    uint16_t OVPhaseCIntEN;
    uint16_t OVPhaseBIntEN;
    uint16_t OVPhaseAIntEN;
    uint16_t OIPhaseCIntEN;
    uint16_t OIPhaseBIntEN;
    uint16_t OIPhaseAIntEN;
    uint16_t PERegAPIntEn;
    uint16_t PERegBPIntEn;
    uint16_t PERegCPIntEn;
    uint16_t PERegTPIntEn;
    uint16_t QERegAPIntEn;
    uint16_t QERegBPIntEn;
    uint16_t QERegCPIntEn;
    uint16_t QERgTPIntEn;
    uint16_t PhaseLossCIntEn;
    uint16_t PhaseLossBIntEn;
    uint16_t PhaseLossAIntEn;
    uint16_t FreqLoIntEn;
    uint16_t SagPhaseCIntEn;
    uint16_t SagPhaseBIntEn;
    uint16_t SagPhaseAIntEn;
    uint16_t FreqHiIntEn;
};

struct EnergyAccumulationSettings {
    unsigned long energyReadInterval;   // Milliseconds between energy register reads
    unsigned long energySaveInterval;   // Milliseconds between saves to SD card
    double accumulatedEnergyA;          // Accumulated energy in kWh for phase A
    double accumulatedEnergyB;          // Accumulated energy in kWh for phase B
    double accumulatedEnergyC;          // Accumulated energy in kWh for phase C
    uint16_t meterConstant;             // Meter constant (imp/kWh) for energy accumulation
};





class SettingsManager {
public:
    SettingsManager(RegisterAccess& regAccess);
    
    // Load/Save settings
    bool loadSettings();
    bool saveSettings();
    
    //getter/setter for all settings
    const WiFiSettings& getWiFiSettings() { return _wifi; }
    void setWiFiSettings(const WiFiSettings& settings) { _wifi = settings; }
    
    const RTCCalibrationSettings& getRTCCalibration() { return _rtcCalibration; }
    void setRTCCalibration(const RTCCalibrationSettings& settings) { _rtcCalibration = settings; }
    
    const TimezoneSettings& getTimezoneSettings() { return _timezone; }
    void setTimezoneSettings(const TimezoneSettings& settings) { _timezone = settings; }
    
    const DataLoggingSettings& getDataLoggingSettings() { return _dataLogging; }
    void setDataLoggingSettings(const DataLoggingSettings& settings) { _dataLogging = settings; }
    
    const DisplaySettings& getDisplaySettings() { return _display; }
    void setDisplaySettings(const DisplaySettings& settings) { _display = settings; }
    
    const SystemSettings& getSystemSettings() { return _system; }
    void setSystemSettings(const SystemSettings& settings) { _system = settings; }
    
    const StatusAndSpecialRegisters& getStatusAndSpecialRegisters() { return _statusAndSpecialRegisters; }
    void setStatusAndSpecialRegisters(const StatusAndSpecialRegisters& settings) { _statusAndSpecialRegisters = settings; }

    const ConfigurationRegisters& getConfigurationRegisters() { return _configurationRegisters; }
    void setConfigurationRegisters(const ConfigurationRegisters& settings) { _configurationRegisters = settings; }

    const CalibrationRegisters& getCalibrationRegisters() { return _calibrationRegisters; }
    void setCalibrationRegisters(const CalibrationRegisters& settings) { _calibrationRegisters = settings; }

    const FundamentalHarmonicCalibrationRegisters& getFundamentalHarmonicCalibrationRegisters() { return _fundamentalHarmonicCalibrationRegisters; }
    void setFundamentalHarmonicCalibrationRegisters(const FundamentalHarmonicCalibrationRegisters& settings) { _fundamentalHarmonicCalibrationRegisters = settings; }

    const MeasurementCalibrationRegisters& getMeasurementCalibrationRegisters() { return _measurementCalibrationRegisters; }
    void setMeasurementCalibrationRegisters(const MeasurementCalibrationRegisters& settings) { _measurementCalibrationRegisters = settings; }

    const EMMStatusRegisters& getEMMStatusRegisters() { return _emmStatusRegisters; }
    void setEMMStatusRegisters(const EMMStatusRegisters& settings) { _emmStatusRegisters = settings; }

    const EnergyAccumulationSettings& getEnergyAccumulationSettings() { return _energyAccumulation; }
    void setEnergyAccumulationSettings(const EnergyAccumulationSettings& settings) { _energyAccumulation = settings; }

    // Apply all settings to chip
    bool applyAllRegistersToChip();
    
private:
    RegisterAccess& _regAccess;
    WiFiSettings _wifi;
    RTCCalibrationSettings _rtcCalibration;
    TimezoneSettings _timezone;
    DataLoggingSettings _dataLogging;
    DisplaySettings _display;
    SystemSettings _system;
    EnergyAccumulationSettings _energyAccumulation;
    StatusAndSpecialRegisters _statusAndSpecialRegisters;
    ConfigurationRegisters _configurationRegisters;
    CalibrationRegisters _calibrationRegisters;
    FundamentalHarmonicCalibrationRegisters _fundamentalHarmonicCalibrationRegisters;
    MeasurementCalibrationRegisters _measurementCalibrationRegisters;
    EMMStatusRegisters _emmStatusRegisters;
    
    static const char* SETTINGS_FILE;
    
    // INI file parsing helpers
    String readIniValue(const String& content, const String& section, const String& key);
    bool parseSettings(const String& content);
    String generateSettingsINI();
    String trim(const String& str);
};

#endif