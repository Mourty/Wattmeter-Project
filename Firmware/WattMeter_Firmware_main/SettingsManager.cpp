#include "SettingsManager.h"

const char* SettingsManager::SETTINGS_FILE = "/settings.ini";

SettingsManager::SettingsManager(RegisterAccess& regAccess)
: _regAccess(regAccess) {
    // Initialize WiFi with defaults
    _wifi.ssid = "";
    _wifi.password = "";
    
    // Default RTC calibration settings
    _rtcCalibration.ntpServer = "pool.ntp.org";
    _rtcCalibration.minCalibrationDays = 1;
    _rtcCalibration.calibrationThreshold = 5.0;
    _rtcCalibration.autoCalibrationEnabled = true;
    _rtcCalibration.lastCalibrationTime = 0;
    _rtcCalibration.currentOffset = 0;
    _rtcCalibration.calibrationEnabled = true;
    
    // Default timezone settings (US Central Time)
    _timezone.dstAbbrev = "CDT";
    _timezone.dstWeek = 2;           // Second week
    _timezone.dstDow = 1;            // Sunday (using Timezone library convention: 1=Sun)
    _timezone.dstMonth = 3;          // March
    _timezone.dstHour = 2;           // 2 AM
    _timezone.dstOffset = -300;      // UTC-5 hours
    
    _timezone.stdAbbrev = "CST";
    _timezone.stdWeek = 1;           // First week
    _timezone.stdDow = 1;            // Sunday
    _timezone.stdMonth = 11;         // November
    _timezone.stdHour = 2;           // 2 AM
    _timezone.stdOffset = -360;      // UTC-6 hours

    // Data logging defaults
    _dataLogging.loggingInterval = 1000;           // 1 second
    _dataLogging.bufferSize = 60;                  // 60 readings
    _dataLogging.powerLossThreshold = 100.0;       // 100V
    _dataLogging.enablePowerLossDetection = true;
    _dataLogging.logFields = "UrmsA,IrmsA,PmeanA,SmeanA,QmeanA,Freq";

    // Display defaults
    _display.field0 = "UrmsA";
    _display.field1 = "IrmsA";
    _display.field2 = "PmeanA";
    _display.backlightTimeout = 30000;  // 30 seconds
    _display.longPressTime = 10000;     // 10 seconds

    // System defaults
    _system.autoRebootEnabled = true;
    _system.rebootIntervalHours = 168;  // 1 week
    _system.rebootHour = 3;              // 3 AM

    // Status and Special Registers defaults
    _statusAndSpecialRegisters.IA_SRC = 0x0;
    _statusAndSpecialRegisters.IB_SRC = 0x1;
    _statusAndSpecialRegisters.IC_SRC = 0x2;
    _statusAndSpecialRegisters.UA_SRC = 0x4;
    _statusAndSpecialRegisters.UB_SRC = 0x5;
    _statusAndSpecialRegisters.UC_SRC = 0x6;
    _statusAndSpecialRegisters.Sag_Period = 0x3F;
    _statusAndSpecialRegisters.PeakDet_period = 0x14;
    _statusAndSpecialRegisters.OVth = 0xC000;
    _statusAndSpecialRegisters.Zxdis = 0x1;
    _statusAndSpecialRegisters.ZX0Con = 0x0;
    _statusAndSpecialRegisters.ZX1Con = 0x0;
    _statusAndSpecialRegisters.ZX2Con = 0x0;
    _statusAndSpecialRegisters.ZX0Src = 0x0;
    _statusAndSpecialRegisters.ZX1Src = 0x0;
    _statusAndSpecialRegisters.ZX2Src = 0x0;
    _statusAndSpecialRegisters.SagTh = 0x1000;
    _statusAndSpecialRegisters.PhaseLossTh = 0x0400;
    _statusAndSpecialRegisters.InWarnTh = 0xFFFF;
    _statusAndSpecialRegisters.OIth = 0xC000;
    _statusAndSpecialRegisters.FreqLoTh = 0x170C;
    _statusAndSpecialRegisters.FreqHiTh = 0x17D4;
    _statusAndSpecialRegisters.IRQ1_OR = 0x0;
    _statusAndSpecialRegisters.WARN_OR = 0x0;

    // Configuration Registers defaults
    _configurationRegisters.PL_Constant = 0x0861C468;
    _configurationRegisters.EnPC = 0x1;
    _configurationRegisters.EnPB = 0x1;
    _configurationRegisters.EnPA = 0x1;
    _configurationRegisters.ABSEnP = 0x0;
    _configurationRegisters.ABSEnQ = 0x0;
    _configurationRegisters.CF2varh = 0x1;
    _configurationRegisters._3P3W = 0x0;
    _configurationRegisters.didtEn = 0x0;
    _configurationRegisters.HPFoff = 0x0;
    _configurationRegisters.Freq60Hz = 0x1;
    _configurationRegisters.PGA_GAIN = 0x0000;
    _configurationRegisters.PStartTh = 0x0000;
    _configurationRegisters.QStartTh = 0x0000;
    _configurationRegisters.SStartTh = 0x0000;
    _configurationRegisters.PPhaseTh = 0x0000;
    _configurationRegisters.QPhaseTh = 0x0000;
    _configurationRegisters.SPhaseTh = 0x0000;

    // Calibration Registers defaults (energy calibration)
    _calibrationRegisters.PoffsetA = 0x0000;
    _calibrationRegisters.QoffsetA = 0x0000;
    _calibrationRegisters.PoffsetB = 0x0000;
    _calibrationRegisters.QoffsetB = 0x0000;
    _calibrationRegisters.PoffsetC = 0x0000;
    _calibrationRegisters.QoffsetC = 0x0000;
    _calibrationRegisters.PQGainA = 0x0000;
    _calibrationRegisters.PhiA = 0x0000;
    _calibrationRegisters.PQGainB = 0x0000;
    _calibrationRegisters.PhiB = 0x0000;
    _calibrationRegisters.PQGainC = 0x0000;
    _calibrationRegisters.PhiC = 0x0000;

    // Fundamental Harmonic Calibration Registers defaults
    _fundamentalHarmonicCalibrationRegisters.PoffsetAF = 0x0000;
    _fundamentalHarmonicCalibrationRegisters.PoffsetBF = 0x0000;
    _fundamentalHarmonicCalibrationRegisters.PoffsetCF = 0x0000;
    _fundamentalHarmonicCalibrationRegisters.PGainAF = 0x0000;
    _fundamentalHarmonicCalibrationRegisters.PGainBF = 0x0000;
    _fundamentalHarmonicCalibrationRegisters.PGainCF = 0x0000;

    // Measurement Calibration Registers defaults (RMS calibration)
    _measurementCalibrationRegisters.UgainA = 0x1616;
    _measurementCalibrationRegisters.IgainA = 0x28D0;
    _measurementCalibrationRegisters.UoffsetA = 0x8100;
    _measurementCalibrationRegisters.IoffsetA = 0x0000;
    _measurementCalibrationRegisters.UgainB = 0x8000;
    _measurementCalibrationRegisters.IgainB = 0x8000;
    _measurementCalibrationRegisters.UoffsetB = 0x0000;
    _measurementCalibrationRegisters.IoffsetB = 0x0000;
    _measurementCalibrationRegisters.UgainC = 0x8000;
    _measurementCalibrationRegisters.IgainC = 0x8000;
    _measurementCalibrationRegisters.UoffsetC = 0x0000;
    _measurementCalibrationRegisters.IoffsetC = 0x0000;

    // EMM Status Registers defaults (all interrupt enables disabled)
    _emmStatusRegisters.CF4RevIntEN = 0x0;
    _emmStatusRegisters.CF3RevIntEN = 0x0;
    _emmStatusRegisters.CF2RevIntEN = 0x0;
    _emmStatusRegisters.CF1RevIntEN = 0x0;
    _emmStatusRegisters.TASNoloadIntEN = 0x0;
    _emmStatusRegisters.TPNoloadIntEN = 0x0;
    _emmStatusRegisters.TQNoloadIntEN = 0x0;
    _emmStatusRegisters.INOv0IntEN = 0x0;
    _emmStatusRegisters.IRevWnIntEN = 0x0;
    _emmStatusRegisters.URevWnIntEN = 0x0;
    _emmStatusRegisters.OVPhaseCIntEN = 0x0;
    _emmStatusRegisters.OVPhaseBIntEN = 0x0;
    _emmStatusRegisters.OVPhaseAIntEN = 0x0;
    _emmStatusRegisters.OIPhaseCIntEN = 0x0;
    _emmStatusRegisters.OIPhaseBIntEN = 0x0;
    _emmStatusRegisters.OIPhaseAIntEN = 0x0;
    _emmStatusRegisters.PERegAPIntEn = 0x0;
    _emmStatusRegisters.PERegBPIntEn = 0x0;
    _emmStatusRegisters.PERegCPIntEn = 0x0;
    _emmStatusRegisters.PERegTPIntEn = 0x0;
    _emmStatusRegisters.QERegAPIntEn = 0x0;
    _emmStatusRegisters.QERegBPIntEn = 0x0;
    _emmStatusRegisters.QERegCPIntEn = 0x0;
    _emmStatusRegisters.QERgTPIntEn = 0x0;
    _emmStatusRegisters.PhaseLossCIntEn = 0x0;
    _emmStatusRegisters.PhaseLossBIntEn = 0x0;
    _emmStatusRegisters.PhaseLossAIntEn = 0x0;
    _emmStatusRegisters.FreqLoIntEn = 0x0;
    _emmStatusRegisters.SagPhaseCIntEn = 0x0;
    _emmStatusRegisters.SagPhaseBIntEn = 0x0;
    _emmStatusRegisters.SagPhaseAIntEn = 0x0;
    _emmStatusRegisters.FreqHiIntEn = 0x0;

    // Energy accumulation defaults
    _energyAccumulation.energyReadInterval = 20000;    // 20 seconds
    _energyAccumulation.energySaveInterval = 600000;   // 10 minutes
    _energyAccumulation.accumulatedEnergyA = 0.0;
    _energyAccumulation.accumulatedEnergyB = 0.0;
    _energyAccumulation.accumulatedEnergyC = 0.0;
    _energyAccumulation.meterConstant = 3200;        // Default meter constant
}

bool SettingsManager::loadSettings() {
    if (!SD.exists(SETTINGS_FILE)) {
        Serial.println("Settings file not found, using defaults");
        return false;
    }
    
    File file = SD.open(SETTINGS_FILE, FILE_READ);
    if (!file) {
        Serial.println("Failed to open settings file");
        return false;
    }
    
    String content = "";
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    
    bool success = parseSettings(content);
    if (success) {
        Serial.println("Settings loaded successfully");
    } else {
        Serial.println("Failed to parse settings file");
    }
    
    return success;
}

bool SettingsManager::saveSettings() {
    String content = generateSettingsINI();
    
    File file = SD.open(SETTINGS_FILE, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open settings file for writing");
        return false;
    }
    
    file.print(content);
    file.close();
    
    Serial.println("Settings saved successfully");
    return true;
}


String SettingsManager::readIniValue(const String& content, const String& section, const String& key) {
    // Find section
    String sectionHeader = "[" + section + "]";
    int sectionStart = content.indexOf(sectionHeader);
    if (sectionStart == -1) {
        return "";
    }
    
    // Find next section or end of file
    int sectionEnd = content.indexOf("[", sectionStart + 1);
    if (sectionEnd == -1) {
        sectionEnd = content.length();
    }
    
    // Search for key within section
    String sectionContent = content.substring(sectionStart, sectionEnd);
    String keyStr = key + "=";
    int keyStart = sectionContent.indexOf(keyStr);
    if (keyStart == -1) {
        return "";
    }
    
    // Extract value (everything after = until newline)
    int valueStart = keyStart + keyStr.length();
    int valueEnd = sectionContent.indexOf("\n", valueStart);
    if (valueEnd == -1) {
        valueEnd = sectionContent.length();
    }
    
    String value = sectionContent.substring(valueStart, valueEnd);
    return trim(value);
}

bool SettingsManager::parseSettings(const String& content) {
    String val;
    
    // Parse WiFi section
    val = readIniValue(content, "WiFi", "SSID");
    if (val.length() > 0) _wifi.ssid = val;

    val = readIniValue(content, "WiFi", "Password");
    if (val.length() > 0) _wifi.password = val;

    // Parse RTC Calibration section
    val = readIniValue(content, "RTCCalibration", "NTPServer");
    if (val.length() > 0) _rtcCalibration.ntpServer = val;
    
    val = readIniValue(content, "RTCCalibration", "MinCalibrationDays");
    if (val.length() > 0) _rtcCalibration.minCalibrationDays = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "RTCCalibration", "CalibrationThreshold");
    if (val.length() > 0) _rtcCalibration.calibrationThreshold = atof(val.c_str());
    
    val = readIniValue(content, "RTCCalibration", "AutoCalibrationEnabled");
    if (val.length() > 0) _rtcCalibration.autoCalibrationEnabled = (val == "1" || val == "true");
    
    val = readIniValue(content, "RTCCalibration", "CalibrationEnabled");
    if (val.length() > 0) _rtcCalibration.calibrationEnabled = (val == "1" || val == "true");
    
    val = readIniValue(content, "RTCCalibration", "LastCalibrationTime");
    if (val.length() > 0) _rtcCalibration.lastCalibrationTime = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "RTCCalibration", "CurrentOffset");
    if (val.length() > 0) _rtcCalibration.currentOffset = strtol(val.c_str(), NULL, 0);
    
    // Parse Timezone section
    val = readIniValue(content, "Timezone", "DSTAbbrev");
    if (val.length() > 0) _timezone.dstAbbrev = val;
    
    val = readIniValue(content, "Timezone", "DSTWeek");
    if (val.length() > 0) _timezone.dstWeek = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "DSTDOW");
    if (val.length() > 0) _timezone.dstDow = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "DSTMonth");
    if (val.length() > 0) _timezone.dstMonth = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "DSTHour");
    if (val.length() > 0) _timezone.dstHour = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "DSTOffset");
    if (val.length() > 0) _timezone.dstOffset = strtol(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "STDAbbrev");
    if (val.length() > 0) _timezone.stdAbbrev = val;
    
    val = readIniValue(content, "Timezone", "STDWeek");
    if (val.length() > 0) _timezone.stdWeek = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "STDDOW");
    if (val.length() > 0) _timezone.stdDow = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "STDMonth");
    if (val.length() > 0) _timezone.stdMonth = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "STDHour");
    if (val.length() > 0) _timezone.stdHour = strtol(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Timezone", "STDOffset");
    if (val.length() > 0) _timezone.stdOffset = strtol(val.c_str(), NULL, 0);
    
    // Parse DataLogging section
    val = readIniValue(content, "DataLogging", "LoggingInterval");
    if (val.length() > 0) _dataLogging.loggingInterval = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "DataLogging", "BufferSize");
    if (val.length() > 0) {
        unsigned int size = strtoul(val.c_str(), NULL, 0);
        // Clamp to safe range (1-1000)
        if (size < 1) size = 1;
        if (size > 1000) size = 1000;
        _dataLogging.bufferSize = size;
    }
    
    val = readIniValue(content, "DataLogging", "PowerLossThreshold");
    if (val.length() > 0) _dataLogging.powerLossThreshold = atof(val.c_str());
    
    val = readIniValue(content, "DataLogging", "EnablePowerLossDetection");
    if (val.length() > 0) _dataLogging.enablePowerLossDetection = (val == "1" || val == "true");

    val = readIniValue(content, "DataLogging", "LogFields");
    if (val.length() > 0) _dataLogging.logFields = val;

    // Parse Display section
    val = readIniValue(content, "Display", "Field0");
    if (val.length() > 0) _display.field0 = val;
    
    val = readIniValue(content, "Display", "Field1");
    if (val.length() > 0) _display.field1 = val;
    
    val = readIniValue(content, "Display", "Field2");
    if (val.length() > 0) _display.field2 = val;
    
    val = readIniValue(content, "Display", "BacklightTimeout");
    if (val.length() > 0) _display.backlightTimeout = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "Display", "LongPressTime");
    if (val.length() > 0) _display.longPressTime = strtoul(val.c_str(), NULL, 0);

    // Parse System section
    val = readIniValue(content, "System", "AutoRebootEnabled");
    if (val.length() > 0) _system.autoRebootEnabled = (val == "1" || val == "true");
    
    val = readIniValue(content, "System", "RebootIntervalHours");
    if (val.length() > 0) _system.rebootIntervalHours = strtoul(val.c_str(), NULL, 0);
    
    val = readIniValue(content, "System", "RebootHour");
    if (val.length() > 0) _system.rebootHour = strtol(val.c_str(), NULL, 0);

    // Parse Energy_Accumulation section
    val = readIniValue(content, "Energy_Accumulation", "EnergyReadInterval");
    if (val.length() > 0) _energyAccumulation.energyReadInterval = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Energy_Accumulation", "EnergySaveInterval");
    if (val.length() > 0) _energyAccumulation.energySaveInterval = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Energy_Accumulation", "AccumulatedEnergyA");
    if (val.length() > 0) _energyAccumulation.accumulatedEnergyA = atof(val.c_str());

    val = readIniValue(content, "Energy_Accumulation", "AccumulatedEnergyB");
    if (val.length() > 0) _energyAccumulation.accumulatedEnergyB = atof(val.c_str());

    val = readIniValue(content, "Energy_Accumulation", "AccumulatedEnergyC");
    if (val.length() > 0) _energyAccumulation.accumulatedEnergyC = atof(val.c_str());

    val = readIniValue(content, "Energy_Accumulation", "MeterConstant");
    if (val.length() > 0) _energyAccumulation.meterConstant = strtoul(val.c_str(), NULL, 0);

    // Parse Status_and_Special_Registers section
    val = readIniValue(content, "Status_and_Special_Registers", "IA_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.IA_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "IB_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.IB_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "IC_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.IC_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "UA_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.UA_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "UB_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.UB_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "UC_SRC");
    if (val.length() > 0) _statusAndSpecialRegisters.UC_SRC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "Sag_Period");
    if (val.length() > 0) _statusAndSpecialRegisters.Sag_Period = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "PeakDet_period");
    if (val.length() > 0) _statusAndSpecialRegisters.PeakDet_period = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "OVth");
    if (val.length() > 0) _statusAndSpecialRegisters.OVth = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "Zxdis");
    if (val.length() > 0) _statusAndSpecialRegisters.Zxdis = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX0Con");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX0Con = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX1Con");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX1Con = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX2Con");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX2Con = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX0Src");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX0Src = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX1Src");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX1Src = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "ZX2Src");
    if (val.length() > 0) _statusAndSpecialRegisters.ZX2Src = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "SagTh");
    if (val.length() > 0) _statusAndSpecialRegisters.SagTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "PhaseLossTh");
    if (val.length() > 0) _statusAndSpecialRegisters.PhaseLossTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "InWarnTh");
    if (val.length() > 0) _statusAndSpecialRegisters.InWarnTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "OIth");
    if (val.length() > 0) _statusAndSpecialRegisters.OIth = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "FreqLoTh");
    if (val.length() > 0) _statusAndSpecialRegisters.FreqLoTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "FreqHiTh");
    if (val.length() > 0) _statusAndSpecialRegisters.FreqHiTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "IRQ1_OR");
    if (val.length() > 0) _statusAndSpecialRegisters.IRQ1_OR = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Status_and_Special_Registers", "WARN_OR");
    if (val.length() > 0) _statusAndSpecialRegisters.WARN_OR = strtoul(val.c_str(), NULL, 0);

    // Parse Configuration_Registers section
    val = readIniValue(content, "Configuration_Registers", "PL_Constant");
    if (val.length() > 0) _configurationRegisters.PL_Constant = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "EnPC");
    if (val.length() > 0) _configurationRegisters.EnPC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "EnPB");
    if (val.length() > 0) _configurationRegisters.EnPB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "EnPA");
    if (val.length() > 0) _configurationRegisters.EnPA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "ABSEnP");
    if (val.length() > 0) _configurationRegisters.ABSEnP = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "ABSEnQ");
    if (val.length() > 0) _configurationRegisters.ABSEnQ = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "CF2varh");
    if (val.length() > 0) _configurationRegisters.CF2varh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "3P3W");
    if (val.length() > 0) _configurationRegisters._3P3W = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "didtEn");
    if (val.length() > 0) _configurationRegisters.didtEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "HPFoff");
    if (val.length() > 0) _configurationRegisters.HPFoff = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "Freq60Hz");
    if (val.length() > 0) _configurationRegisters.Freq60Hz = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "PGA_GAIN");
    if (val.length() > 0) _configurationRegisters.PGA_GAIN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "PStartTh");
    if (val.length() > 0) _configurationRegisters.PStartTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "QStartTh");
    if (val.length() > 0) _configurationRegisters.QStartTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "SStartTh");
    if (val.length() > 0) _configurationRegisters.SStartTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "PPhaseTh");
    if (val.length() > 0) _configurationRegisters.PPhaseTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "QPhaseTh");
    if (val.length() > 0) _configurationRegisters.QPhaseTh = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Configuration_Registers", "SPhaseTh");
    if (val.length() > 0) _configurationRegisters.SPhaseTh = strtoul(val.c_str(), NULL, 0);

    // Parse Calibration_Registers section
    val = readIniValue(content, "Calibration_Registers", "PoffsetA");
    if (val.length() > 0) _calibrationRegisters.PoffsetA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "QoffsetA");
    if (val.length() > 0) _calibrationRegisters.QoffsetA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PoffsetB");
    if (val.length() > 0) _calibrationRegisters.PoffsetB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "QoffsetB");
    if (val.length() > 0) _calibrationRegisters.QoffsetB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PoffsetC");
    if (val.length() > 0) _calibrationRegisters.PoffsetC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "QoffsetC");
    if (val.length() > 0) _calibrationRegisters.QoffsetC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PQGainA");
    if (val.length() > 0) _calibrationRegisters.PQGainA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PhiA");
    if (val.length() > 0) _calibrationRegisters.PhiA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PQGainB");
    if (val.length() > 0) _calibrationRegisters.PQGainB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PhiB");
    if (val.length() > 0) _calibrationRegisters.PhiB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PQGainC");
    if (val.length() > 0) _calibrationRegisters.PQGainC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Calibration_Registers", "PhiC");
    if (val.length() > 0) _calibrationRegisters.PhiC = strtoul(val.c_str(), NULL, 0);

    // Parse Fundamental_Harmonic_Energy_Calibration_Registers section
    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PoffsetAF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PoffsetAF = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PoffsetBF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PoffsetBF = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PoffsetCF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PoffsetCF = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PGainAF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PGainAF = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PGainBF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PGainBF = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Fundamental_Harmonic_Energy_Calibration_Registers", "PGainCF");
    if (val.length() > 0) _fundamentalHarmonicCalibrationRegisters.PGainCF = strtoul(val.c_str(), NULL, 0);

    // Parse Measurement_Calibration_Registers section
    val = readIniValue(content, "Measurement_Calibration_Registers", "UgainA");
    if (val.length() > 0) _measurementCalibrationRegisters.UgainA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IgainA");
    if (val.length() > 0) _measurementCalibrationRegisters.IgainA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "UoffsetA");
    if (val.length() > 0) _measurementCalibrationRegisters.UoffsetA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IoffsetA");
    if (val.length() > 0) _measurementCalibrationRegisters.IoffsetA = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "UgainB");
    if (val.length() > 0) _measurementCalibrationRegisters.UgainB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IgainB");
    if (val.length() > 0) _measurementCalibrationRegisters.IgainB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "UoffsetB");
    if (val.length() > 0) _measurementCalibrationRegisters.UoffsetB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IoffsetB");
    if (val.length() > 0) _measurementCalibrationRegisters.IoffsetB = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "UgainC");
    if (val.length() > 0) _measurementCalibrationRegisters.UgainC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IgainC");
    if (val.length() > 0) _measurementCalibrationRegisters.IgainC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "UoffsetC");
    if (val.length() > 0) _measurementCalibrationRegisters.UoffsetC = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "Measurement_Calibration_Registers", "IoffsetC");
    if (val.length() > 0) _measurementCalibrationRegisters.IoffsetC = strtoul(val.c_str(), NULL, 0);

    // Parse EMM_Status_Registers section
    val = readIniValue(content, "EMM_Status_Registers", "CF4RevIntEN");
    if (val.length() > 0) _emmStatusRegisters.CF4RevIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "CF3RevIntEN");
    if (val.length() > 0) _emmStatusRegisters.CF3RevIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "CF2RevIntEN");
    if (val.length() > 0) _emmStatusRegisters.CF2RevIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "CF1RevIntEN");
    if (val.length() > 0) _emmStatusRegisters.CF1RevIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "TASNoloadIntEN");
    if (val.length() > 0) _emmStatusRegisters.TASNoloadIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "TPNoloadIntEN");
    if (val.length() > 0) _emmStatusRegisters.TPNoloadIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "TQNoloadIntEN");
    if (val.length() > 0) _emmStatusRegisters.TQNoloadIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "INOv0IntEN");
    if (val.length() > 0) _emmStatusRegisters.INOv0IntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "IRevWnIntEN");
    if (val.length() > 0) _emmStatusRegisters.IRevWnIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "URevWnIntEN");
    if (val.length() > 0) _emmStatusRegisters.URevWnIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OVPhaseCIntEN");
    if (val.length() > 0) _emmStatusRegisters.OVPhaseCIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OVPhaseBIntEN");
    if (val.length() > 0) _emmStatusRegisters.OVPhaseBIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OVPhaseAIntEN");
    if (val.length() > 0) _emmStatusRegisters.OVPhaseAIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OIPhaseCIntEN");
    if (val.length() > 0) _emmStatusRegisters.OIPhaseCIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OIPhaseBIntEN");
    if (val.length() > 0) _emmStatusRegisters.OIPhaseBIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "OIPhaseAIntEN");
    if (val.length() > 0) _emmStatusRegisters.OIPhaseAIntEN = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PERegAPIntEn");
    if (val.length() > 0) _emmStatusRegisters.PERegAPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PERegBPIntEn");
    if (val.length() > 0) _emmStatusRegisters.PERegBPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PERegCPIntEn");
    if (val.length() > 0) _emmStatusRegisters.PERegCPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PERegTPIntEn");
    if (val.length() > 0) _emmStatusRegisters.PERegTPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "QERegAPIntEn");
    if (val.length() > 0) _emmStatusRegisters.QERegAPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "QERegBPIntEn");
    if (val.length() > 0) _emmStatusRegisters.QERegBPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "QERegCPIntEn");
    if (val.length() > 0) _emmStatusRegisters.QERegCPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "QERgTPIntEn");
    if (val.length() > 0) _emmStatusRegisters.QERgTPIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PhaseLossCIntEn");
    if (val.length() > 0) _emmStatusRegisters.PhaseLossCIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PhaseLossBIntEn");
    if (val.length() > 0) _emmStatusRegisters.PhaseLossBIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "PhaseLossAIntEn");
    if (val.length() > 0) _emmStatusRegisters.PhaseLossAIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "FreqLoIntEn");
    if (val.length() > 0) _emmStatusRegisters.FreqLoIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "SagPhaseCIntEn");
    if (val.length() > 0) _emmStatusRegisters.SagPhaseCIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "SagPhaseBIntEn");
    if (val.length() > 0) _emmStatusRegisters.SagPhaseBIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "SagPhaseAIntEn");
    if (val.length() > 0) _emmStatusRegisters.SagPhaseAIntEn = strtoul(val.c_str(), NULL, 0);

    val = readIniValue(content, "EMM_Status_Registers", "FreqHiIntEn");
    if (val.length() > 0) _emmStatusRegisters.FreqHiIntEn = strtoul(val.c_str(), NULL, 0);

    return true;
}

String SettingsManager::generateSettingsINI() {
    String ini = "";
    
    // WiFi section
    ini += "[WiFi]\n";
    ini += "SSID=" + _wifi.ssid + "\n";
    ini += "Password=" + _wifi.password + "\n";
    ini += "\n";

    // RTC Calibration section
    ini += "[RTCCalibration]\n";
    ini += "NTPServer=" + _rtcCalibration.ntpServer + "\n";
    ini += "MinCalibrationDays=" + String(_rtcCalibration.minCalibrationDays) + "\n";
    ini += "CalibrationThreshold=" + String(_rtcCalibration.calibrationThreshold, 1) + "\n";
    ini += "AutoCalibrationEnabled=" + String(_rtcCalibration.autoCalibrationEnabled ? "1" : "0") + "\n";
    ini += "CalibrationEnabled=" + String(_rtcCalibration.calibrationEnabled ? "1" : "0") + "\n";
    ini += "LastCalibrationTime=" + String(_rtcCalibration.lastCalibrationTime) + "\n";
    ini += "CurrentOffset=" + String(_rtcCalibration.currentOffset) + "\n";
    ini += "\n";
    
    // Timezone section
    ini += "[Timezone]\n";
    ini += "; Daylight/Summer Time\n";
    ini += "DSTAbbrev=" + _timezone.dstAbbrev + "\n";
    ini += "DSTWeek=" + String(_timezone.dstWeek) + "\t; 1=First, 2=Second, 3=Third, 4=Fourth, 5=Last\n";
    ini += "DSTDOW=" + String(_timezone.dstDow) + "\t; 1=Sun, 2=Mon, 3=Tue, 4=Wed, 5=Thu, 6=Fri, 7=Sat\n";
    ini += "DSTMonth=" + String(_timezone.dstMonth) + "\t; 1-12\n";
    ini += "DSTHour=" + String(_timezone.dstHour) + "\t; 0-23\n";
    ini += "DSTOffset=" + String(_timezone.dstOffset) + "\t; Minutes from UTC (e.g., -300 for UTC-5)\n";
    ini += "\n";
    ini += "; Standard Time\n";
    ini += "STDAbbrev=" + _timezone.stdAbbrev + "\n";
    ini += "STDWeek=" + String(_timezone.stdWeek) + "\n";
    ini += "STDDOW=" + String(_timezone.stdDow) + "\n";
    ini += "STDMonth=" + String(_timezone.stdMonth) + "\n";
    ini += "STDHour=" + String(_timezone.stdHour) + "\n";
    ini += "STDOffset=" + String(_timezone.stdOffset) + "\n";
    ini += "\n";

    // Data Logging section
    ini += "[DataLogging]\n";
    ini += "LoggingInterval=" + String(_dataLogging.loggingInterval) + "\n";
    ini += "BufferSize=" + String(_dataLogging.bufferSize) + "\n";
    ini += "PowerLossThreshold=" + String(_dataLogging.powerLossThreshold, 1) + "\n";
    ini += "EnablePowerLossDetection=" + String(_dataLogging.enablePowerLossDetection ? "1" : "0") + "\n";
    ini += "LogFields=" + _dataLogging.logFields + "\n";
    ini += "\n";

    // Display section
    ini += "[Display]\n";
    ini += "Field0=" + _display.field0 + "\n";
    ini += "Field1=" + _display.field1 + "\n";
    ini += "Field2=" + _display.field2 + "\n";
    ini += "BacklightTimeout=" + String(_display.backlightTimeout) + "\n";
    ini += "LongPressTime=" + String(_display.longPressTime) + "\n";
    ini += "\n";

    // System section
    ini += "[System]\n";
    ini += "AutoRebootEnabled=" + String(_system.autoRebootEnabled ? "1" : "0") + "\n";
    ini += "RebootIntervalHours=" + String(_system.rebootIntervalHours) + "\n";
    ini += "RebootHour=" + String(_system.rebootHour) + "\n";
    ini += "\n";

    // Energy_Accumulation section (note: uses underscores now)
    ini += "[Energy_Accumulation]\n";
    ini += "EnergyReadInterval=" + String(_energyAccumulation.energyReadInterval) + "\t; ms between energy register reads\n";
    ini += "EnergySaveInterval=" + String(_energyAccumulation.energySaveInterval) + "\t; ms between saves to SD card\n";
    ini += "AccumulatedEnergyA=" + String(_energyAccumulation.accumulatedEnergyA, 6) + "\t; kWh\n";
    ini += "AccumulatedEnergyB=" + String(_energyAccumulation.accumulatedEnergyB, 6) + "\t; kWh\n";
    ini += "AccumulatedEnergyC=" + String(_energyAccumulation.accumulatedEnergyC, 6) + "\t; kWh\n";
    char hexBuf[8];
    sprintf(hexBuf, "0x%04X", _energyAccumulation.meterConstant);
    ini += "MeterConstant=" + String(hexBuf) + "\t; imp/kWh\n";
    ini += "\n";

    // Status_and_Special_Registers section
    ini += "[Status_and_Special_Registers]\n";
    ini += "IA_SRC=0x" + String(_statusAndSpecialRegisters.IA_SRC, HEX) + "\t;ADC Input source for phase A current channel (Default: 0x0)\n";
    ini += "IB_SRC=0x" + String(_statusAndSpecialRegisters.IB_SRC, HEX) + "\t;ADC Input source for phase B current channel (Default: 0x1)\n";
    ini += "IC_SRC=0x" + String(_statusAndSpecialRegisters.IC_SRC, HEX) + "\t;ADC Input source for phase C current channel (Default: 0x2)\n";
    ini += "UA_SRC=0x" + String(_statusAndSpecialRegisters.UA_SRC, HEX) + "\t;ADC Input source for phase A voltage channel (Default: 0x6)\n";
    ini += "UB_SRC=0x" + String(_statusAndSpecialRegisters.UB_SRC, HEX) + "\t;ADC Input source for phase B voltage channel (Default: 0x5)\n";
    ini += "UC_SRC=0x" + String(_statusAndSpecialRegisters.UC_SRC, HEX) + "\t;ADC Input source for phase C voltage channel (Default: 0x4)\n";
    ini += "Sag_Period=0x" + String(_statusAndSpecialRegisters.Sag_Period, HEX) + "\t;Voltage sag period (Default: 0x3F)\n";
    ini += "PeakDet_period=0x" + String(_statusAndSpecialRegisters.PeakDet_period, HEX) + "\t;Peak detect period (Default: 0x14)\n";
    ini += "OVth=0x" + String(_statusAndSpecialRegisters.OVth, HEX) + "\t; Over voltage threshold (Default: 0xC000)\n";
    ini += "Zxdis=0x" + String(_statusAndSpecialRegisters.Zxdis, HEX) + "\t;Zero-crossing Signal Disable (Default: 0x1)\n";
    ini += "ZX0Con=0x" + String(_statusAndSpecialRegisters.ZX0Con, HEX) + "\t;ZX 0 Config (Default: 0x0)\n";
    ini += "ZX1Con=0x" + String(_statusAndSpecialRegisters.ZX1Con, HEX) + "\t;ZX 1 Config (Default: 0x0)\n";
    ini += "ZX2Con=0x" + String(_statusAndSpecialRegisters.ZX2Con, HEX) + "\t;ZX 2 Config (Default: 0x0)\n";
    ini += "ZX0Src=0x" + String(_statusAndSpecialRegisters.ZX0Src, HEX) + "\t;ZX 0 Source (Default: 0x0)\n";
    ini += "ZX1Src=0x" + String(_statusAndSpecialRegisters.ZX1Src, HEX) + "\t;ZX 1 Source (Default: 0x0)\n";
    ini += "ZX2Src=0x" + String(_statusAndSpecialRegisters.ZX2Src, HEX) + "\t;ZX 2 Source (Default: 0x0)\n";
    ini += "SagTh=0x" + String(_statusAndSpecialRegisters.SagTh, HEX) + "\t; Voltage sag threshold (Default: 0x1000)\n";
    ini += "PhaseLossTh=0x" + String(_statusAndSpecialRegisters.PhaseLossTh, HEX) + "\t; Voltage phase loss threshold (Default: 0x0400)\n";
    ini += "InWarnTh=0x" + String(_statusAndSpecialRegisters.InWarnTh, HEX) + "\t; Neutral current warning threshold (Default: 0xFFFF)\n";
    ini += "OIth=0x" + String(_statusAndSpecialRegisters.OIth, HEX) + "\t; Over current threshold (Default: 0xC000)\n";
    ini += "FreqLoTh=0x" + String(_statusAndSpecialRegisters.FreqLoTh, HEX) + "\t; Frequency low threshold (Default: 0x170C)\n";
    ini += "FreqHiTh=0x" + String(_statusAndSpecialRegisters.FreqHiTh, HEX) + "\t; Frequency high threshold (Default: 0x17D4)\n";
    ini += "IRQ1_OR=0x" + String(_statusAndSpecialRegisters.IRQ1_OR, HEX) + "\t;IRQ1 OR with IRQ0 (Default: 0)\n";
    ini += "WARN_OR=0x" + String(_statusAndSpecialRegisters.WARN_OR, HEX) + "\t;WARN OR with IRQ0 (Default: 0)\n";
    ini += "\n";

    // Configuration_Registers section
    ini += "[Configuration_Registers]\n";
    ini += "PL_Constant=0x" + String(_configurationRegisters.PL_Constant, HEX) + "\t;PL constant (Default: 0x0861C468)\n";
    ini += "EnPC=0x" + String(_configurationRegisters.EnPC, HEX) + "\t;Phase C all-Phase Sum energy enable (Default: 0x1)\n";
    ini += "EnPB=0x" + String(_configurationRegisters.EnPB, HEX) + "\t;Phase B all-Phase Sum energy enable (Default: 0x1)\n";
    ini += "EnPA=0x" + String(_configurationRegisters.EnPA, HEX) + "\t;Phase A all-Phase Sum energy enable (Default: 0x1)\n";
    ini += "ABSEnP=0x" + String(_configurationRegisters.ABSEnP, HEX) + "\t;Active Power summing method (Default: 0x0)\n";
    ini += "ABSEnQ=0x" + String(_configurationRegisters.ABSEnQ, HEX) + "\t;Reactive Power summing method (Default: 0x0)\n";
    ini += "CF2varh=0x" + String(_configurationRegisters.CF2varh, HEX) + "\t;CF2 Pin Source (Default: 0x1)\n";
    ini += "3P3W=0x" + String(_configurationRegisters._3P3W, HEX) + "\t;3 or 4 wire 3 phase mode\t (Default: 0x0)\n";
    ini += "didtEn=0x" + String(_configurationRegisters.didtEn, HEX) + "\t;Enable Integrator for didt current sensor (Default: 0x0)\n";
    ini += "HPFoff=0x" + String(_configurationRegisters.HPFoff, HEX) + "\t;Disable HPF Signal processing (Default: 0x0)\n";
    ini += "Freq60Hz=0x" + String(_configurationRegisters.Freq60Hz, HEX) + "\t;Grid frequency flag (Default: 0x1)\n";
    ini += "PGA_GAIN=0x" + String(_configurationRegisters.PGA_GAIN, HEX) + "\t;PGA Gain Config (Default 0x0000)\n";
    ini += "PStartTh=0x" + String(_configurationRegisters.PStartTh, HEX) + "\t; Active startup power threshold (Default: 0x0000)\n";
    ini += "QStartTh=0x" + String(_configurationRegisters.QStartTh, HEX) + "\t; Reactive startup power threshold (Default: 0x0000)\n";
    ini += "SStartTh=0x" + String(_configurationRegisters.SStartTh, HEX) + "\t; Apparent startup power threshold (Default: 0x0000)\n";
    ini += "PPhaseTh=0x" + String(_configurationRegisters.PPhaseTh, HEX) + "\t; Phase active power startup threshold (Default: 0x0000)\n";
    ini += "QPhaseTh=0x" + String(_configurationRegisters.QPhaseTh, HEX) + "\t; Phase reactive power startup threshold (Default: 0x0000)\n";
    ini += "SPhaseTh=0x" + String(_configurationRegisters.SPhaseTh, HEX) + "\t; Phase apparent power startup threshold (Default: 0x0000)\n";
    ini += "\n";

    // Calibration_Registers section
    ini += "[Calibration_Registers]\n";
    ini += "PoffsetA=0x" + String(_calibrationRegisters.PoffsetA, HEX) + "\t; Phase A active power offset (Default: 0x0000)\n";
    ini += "QoffsetA=0x" + String(_calibrationRegisters.QoffsetA, HEX) + "\t; Phase A reactive power offset (Default: 0x0000)\n";
    ini += "PoffsetB=0x" + String(_calibrationRegisters.PoffsetB, HEX) + "\t; Phase B active power offset (Default: 0x0000)\n";
    ini += "QoffsetB=0x" + String(_calibrationRegisters.QoffsetB, HEX) + "\t; Phase B reactive power offset (Default: 0x0000)\n";
    ini += "PoffsetC=0x" + String(_calibrationRegisters.PoffsetC, HEX) + "\t; Phase C active power offset (Default: 0x0000)\n";
    ini += "QoffsetC=0x" + String(_calibrationRegisters.QoffsetC, HEX) + "\t; Phase C reactive power offset (Default: 0x0000)\n";
    ini += "PQGainA=0x" + String(_calibrationRegisters.PQGainA, HEX) + "\t; Phase A energy calibration gain (Default: 0x0000)\n";
    ini += "PhiA=0x" + String(_calibrationRegisters.PhiA, HEX) + "\t; Phase A energy calibration phase angle (Default: 0x0000)\n";
    ini += "PQGainB=0x" + String(_calibrationRegisters.PQGainB, HEX) + "\t; Phase B energy calibration gain (Default: 0x0000)\n";
    ini += "PhiB=0x" + String(_calibrationRegisters.PhiB, HEX) + " \t; Phase B energy calibration phase angle (Default: 0x0000)\n";
    ini += "PQGainC=0x" + String(_calibrationRegisters.PQGainC, HEX) + "\t; Phase C energy calibration gain (Default: 0x0000)\n";
    ini += "PhiC=0x" + String(_calibrationRegisters.PhiC, HEX) + "\t; Phase C energy calibration phase angle (Default: 0x0000)\n";
    ini += "\n";

    // Fundamental_Harmonic_Energy_Calibration_Registers section
    ini += "[Fundamental_Harmonic_Energy_Calibration_Registers]\n";
    ini += "PoffsetAF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PoffsetAF, HEX) + "\t; Phase A fundamental active power offset (Default: 0x0000)\n";
    ini += "PoffsetBF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PoffsetBF, HEX) + "\t; Phase B fundamental active power offset (Default: 0x0000)\n";
    ini += "PoffsetCF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PoffsetCF, HEX) + "\t; Phase C fundamental active power offset (Default: 0x0000)\n";
    ini += "PGainAF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PGainAF, HEX) + "\t; Phase A fundamental calibration gain (Default: 0x0000)\n";
    ini += "PGainBF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PGainBF, HEX) + "\t; Phase B fundamental calibration gain (Default: 0x0000)\n";
    ini += "PGainCF=0x" + String(_fundamentalHarmonicCalibrationRegisters.PGainCF, HEX) + "\t; Phase C fundamental calibration gain (Default: 0x0000)\n";
    ini += "\n";

    // Measurement_Calibration_Registers section
    ini += "[Measurement_Calibration_Registers]\n";
    ini += "UgainA=0x" + String(_measurementCalibrationRegisters.UgainA, HEX) + "\t; Phase A voltage RMS gain (Default: 0x1616)\n";
    ini += "IgainA=0x" + String(_measurementCalibrationRegisters.IgainA, HEX) + "\t; Phase A current RMS gain (Default: 0x28D0)\n";
    ini += "UoffsetA=0x" + String(_measurementCalibrationRegisters.UoffsetA, HEX) + "\t; Phase A voltage RMS offset (Default: 0x8100)\n";
    ini += "IoffsetA=0x" + String(_measurementCalibrationRegisters.IoffsetA, HEX) + "\t; Phase A current RMS offset (Default: 0x0000)\n";
    ini += "UgainB=0x" + String(_measurementCalibrationRegisters.UgainB, HEX) + "\t; Phase B voltage RMS gain (Default: 0x8000)\n";
    ini += "IgainB=0x" + String(_measurementCalibrationRegisters.IgainB, HEX) + "\t; Phase B current RMS gain (Default: 0x8000)\n";
    ini += "UoffsetB=0x" + String(_measurementCalibrationRegisters.UoffsetB, HEX) + "\t; Phase B voltage RMS offset (Default: 0x0000)\n";
    ini += "IoffsetB=0x" + String(_measurementCalibrationRegisters.IoffsetB, HEX) + "\t; Phase B current RMS offset (Default: 0x0000)\n";
    ini += "UgainC=0x" + String(_measurementCalibrationRegisters.UgainC, HEX) + "\t; Phase C voltage RMS gain (Default: 0x8000)\n";
    ini += "IgainC=0x" + String(_measurementCalibrationRegisters.IgainC, HEX) + "\t; Phase C current RMS gain (Default: 0x8000)\n";
    ini += "UoffsetC=0x" + String(_measurementCalibrationRegisters.UoffsetC, HEX) + "\t; Phase C voltage RMS offset (Default: 0x0000)\n";
    ini += "IoffsetC=0x" + String(_measurementCalibrationRegisters.IoffsetC, HEX) + "\t; Phase C current RMS offset (Default: 0x0000)\n";
    ini += "\n";

    // EMM_Status_Registers section
    ini += "[EMM_Status_Registers]\n";
    ini += "CF4RevIntEN=0x" + String(_emmStatusRegisters.CF4RevIntEN, HEX) + "\t;Enable Interrupt Energy for CF4 Forward/Reverse status\t(Default: 0x0)\n";
    ini += "CF3RevIntEN=0x" + String(_emmStatusRegisters.CF3RevIntEN, HEX) + "\t;Enable Interrupt Energy for CF3 Forward/Reverse status\t(Default: 0x0)\n";
    ini += "CF2RevIntEN=0x" + String(_emmStatusRegisters.CF2RevIntEN, HEX) + "\t;Enable Interrupt Energy for CF2 Forward/Reverse status\t(Default: 0x0)\n";
    ini += "CF1RevIntEN=0x" + String(_emmStatusRegisters.CF1RevIntEN, HEX) + "\t;Enable Interrupt Energy for CF1 Forward/Reverse status\t(Default: 0x0)\n";
    ini += "TASNoloadIntEN=0x" + String(_emmStatusRegisters.TASNoloadIntEN, HEX) + "\t;Enable Interrupt All phase sum Apparent Power No load\t(Default: 0x0)\n";
    ini += "TPNoloadIntEN=0x" + String(_emmStatusRegisters.TPNoloadIntEN, HEX) + "\t;Enable Interrupt All phase sum active power no-load\t(Default: 0x0)\n";
    ini += "TQNoloadIntEN=0x" + String(_emmStatusRegisters.TQNoloadIntEN, HEX) + "\t;Enable Interrupt All phase sum reactive power no-load\t(Default: 0x0)\n";
    ini += "INOv0IntEN=0x" + String(_emmStatusRegisters.INOv0IntEN, HEX) + "\t;Enable Interrupt Neural current OV Thresh flag\t(Default: 0x0)\n";
    ini += "IRevWnIntEN=0x" + String(_emmStatusRegisters.IRevWnIntEN, HEX) + "\t;Enable Interrupt Current Phase Sequence Error\t(Default: 0x0)\n";
    ini += "URevWnIntEN=0x" + String(_emmStatusRegisters.URevWnIntEN, HEX) + "\t;Enable Interrupt Voltage Phase Sequence Error\t(Default: 0x0)\n";
    ini += "OVPhaseCIntEN=0x" + String(_emmStatusRegisters.OVPhaseCIntEN, HEX) + "\t;Enable Interrupt Over Voltage Phase C\t(Default: 0x0)\n";
    ini += "OVPhaseBIntEN=0x" + String(_emmStatusRegisters.OVPhaseBIntEN, HEX) + "\t;Enable Interrupt Over Voltage Phase B\t(Default: 0x0)\n";
    ini += "OVPhaseAIntEN=0x" + String(_emmStatusRegisters.OVPhaseAIntEN, HEX) + "\t;Enable Interrupt Over Voltage Phase A\t(Default: 0x0)\n";
    ini += "OIPhaseCIntEN=0x" + String(_emmStatusRegisters.OIPhaseCIntEN, HEX) + "\t;Enable Interrupt Over Current Phase C\t(Default: 0x0)\n";
    ini += "OIPhaseBIntEN=0x" + String(_emmStatusRegisters.OIPhaseBIntEN, HEX) + "\t;Enable Interrupt Over Current Phase B\t(Default: 0x0)\n";
    ini += "OIPhaseAIntEN=0x" + String(_emmStatusRegisters.OIPhaseAIntEN, HEX) + "\t;Enable Interrupt Over Current Phase A\t(Default: 0x0)\n";
    ini += "PERegAPIntEn=0x" + String(_emmStatusRegisters.PERegAPIntEn, HEX) + "\t;Enable Interrupt Active Energy Register C Positive Status\t(Default: 0x0)\n";
    ini += "PERegBPIntEn=0x" + String(_emmStatusRegisters.PERegBPIntEn, HEX) + "\t;Enable Interrupt Active Energy Register B Positive Status\t(Default: 0x0)\n";
    ini += "PERegCPIntEn=0x" + String(_emmStatusRegisters.PERegCPIntEn, HEX) + "\t;Enable Interrupt Active Energy Register A Positive Status\t(Default: 0x0)\n";
    ini += "PERegTPIntEn=0x" + String(_emmStatusRegisters.PERegTPIntEn, HEX) + "\t;Enable Interrupt Active Energy Register Of ABC Positive Status\t(Default: 0x0)\n";
    ini += "QERegAPIntEn=0x" + String(_emmStatusRegisters.QERegAPIntEn, HEX) + "\t;Enable Interrupt Reactive Energy Register C Positive Status\t(Default: 0x0)\n";
    ini += "QERegBPIntEn=0x" + String(_emmStatusRegisters.QERegBPIntEn, HEX) + "\t;Enable Interrupt Reactive Energy Register B Positive Status\t(Default: 0x0)\n";
    ini += "QERegCPIntEn=0x" + String(_emmStatusRegisters.QERegCPIntEn, HEX) + "\t;Enable Interrupt Reactive Energy Register A Positive Status\t(Default: 0x0)\n";
    ini += "QERgTPIntEn=0x" + String(_emmStatusRegisters.QERgTPIntEn, HEX) + " \t;Enable Interrupt Reactive Energy Register Of ABC Positive Status\t(Default: 0x0)\n";
    ini += "PhaseLossCIntEn=0x" + String(_emmStatusRegisters.PhaseLossCIntEn, HEX) + "\t;Enable Interrupt Phase Loss C\t(Default: 0x0)\n";
    ini += "PhaseLossBIntEn=0x" + String(_emmStatusRegisters.PhaseLossBIntEn, HEX) + "\t;Enable Interrupt Phase Loss B\t(Default: 0x0)\n";
    ini += "PhaseLossAIntEn=0x" + String(_emmStatusRegisters.PhaseLossAIntEn, HEX) + "\t;Enable Interrupt Phase Loss A\t(Default: 0x0)\n";
    ini += "FreqLoIntEn=0x" + String(_emmStatusRegisters.FreqLoIntEn, HEX) + "\t;Enable Interrupt Frequency Below Threshold\t(Default: 0x0)\n";
    ini += "SagPhaseCIntEn=0x" + String(_emmStatusRegisters.SagPhaseCIntEn, HEX) + "\t;Enable Interrupt Voltage Sag Phase C\t(Default: 0x0)\n";
    ini += "SagPhaseBIntEn=0x" + String(_emmStatusRegisters.SagPhaseBIntEn, HEX) + "\t;Enable Interrupt Voltage Sag Phase B\t(Default: 0x0)\n";
    ini += "SagPhaseAIntEn=0x" + String(_emmStatusRegisters.SagPhaseAIntEn, HEX) + "\t;Enable Interrupt Voltage Sag Phase A\t(Default: 0x0)\n";
    ini += "FreqHiIntEn=0x" + String(_emmStatusRegisters.FreqHiIntEn, HEX) + "\t;Enable Interrupt Frequency Above Threshold\t(Default: 0x0)\n";

    return ini;
}

bool SettingsManager::applyAllRegistersToChip() {
    // Unlock calibration registers
    bool success = true;
    success &= _regAccess.writeRegister("CfgRegAccEn", 0x55AA);

    Serial.println("Applying all registers to ATM90E32...");

    // Enable the meter
    success &= _regAccess.writeRegister("MeterEn", 1);

    // Apply Status and Special Registers
    success &= _regAccess.writeRegisterRaw("IA_SRC", _statusAndSpecialRegisters.IA_SRC);
    success &= _regAccess.writeRegisterRaw("IB_SRC", _statusAndSpecialRegisters.IB_SRC);
    success &= _regAccess.writeRegisterRaw("IC_SRC", _statusAndSpecialRegisters.IC_SRC);
    success &= _regAccess.writeRegisterRaw("UA_SRC", _statusAndSpecialRegisters.UA_SRC);
    success &= _regAccess.writeRegisterRaw("UB_SRC", _statusAndSpecialRegisters.UB_SRC);
    success &= _regAccess.writeRegisterRaw("UC_SRC", _statusAndSpecialRegisters.UC_SRC);
    success &= _regAccess.writeRegisterRaw("Sag_Period", _statusAndSpecialRegisters.Sag_Period);
    success &= _regAccess.writeRegisterRaw("PeakDet_period", _statusAndSpecialRegisters.PeakDet_period);
    success &= _regAccess.writeRegisterRaw("Ovth", _statusAndSpecialRegisters.OVth);
    success &= _regAccess.writeRegisterRaw("Zxdis", _statusAndSpecialRegisters.Zxdis);
    success &= _regAccess.writeRegisterRaw("ZX0Con", _statusAndSpecialRegisters.ZX0Con);
    success &= _regAccess.writeRegisterRaw("ZX1Con", _statusAndSpecialRegisters.ZX1Con);
    success &= _regAccess.writeRegisterRaw("ZX2Con", _statusAndSpecialRegisters.ZX2Con);
    success &= _regAccess.writeRegisterRaw("ZX0Src", _statusAndSpecialRegisters.ZX0Src);
    success &= _regAccess.writeRegisterRaw("ZX1Src", _statusAndSpecialRegisters.ZX1Src);
    success &= _regAccess.writeRegisterRaw("ZX2Src", _statusAndSpecialRegisters.ZX2Src);
    success &= _regAccess.writeRegisterRaw("SagTh", _statusAndSpecialRegisters.SagTh);
    success &= _regAccess.writeRegisterRaw("PhaseLossTh", _statusAndSpecialRegisters.PhaseLossTh);
    success &= _regAccess.writeRegisterRaw("InWarnTh", _statusAndSpecialRegisters.InWarnTh);
    success &= _regAccess.writeRegisterRaw("Olth", _statusAndSpecialRegisters.OIth);
    success &= _regAccess.writeRegisterRaw("FreqLoTh", _statusAndSpecialRegisters.FreqLoTh);
    success &= _regAccess.writeRegisterRaw("FreqHiTh", _statusAndSpecialRegisters.FreqHiTh);
    success &= _regAccess.writeRegisterRaw("IRQ1_OR", _statusAndSpecialRegisters.IRQ1_OR);
    success &= _regAccess.writeRegisterRaw("WARN_OR", _statusAndSpecialRegisters.WARN_OR);

    if (success) {
        Serial.println("Applied Status and Special Registers");
    } else {
        Serial.println("Failed to apply Status and Special Registers");
    }

    // Apply Configuration Registers
    success &= _regAccess.writeRegisterRaw("PL_Constant", _configurationRegisters.PL_Constant);
    success &= _regAccess.writeRegisterRaw("EnPC", _configurationRegisters.EnPC);
    success &= _regAccess.writeRegisterRaw("EnPB", _configurationRegisters.EnPB);
    success &= _regAccess.writeRegisterRaw("EnPA", _configurationRegisters.EnPA);
    success &= _regAccess.writeRegisterRaw("ABSEnP", _configurationRegisters.ABSEnP);
    success &= _regAccess.writeRegisterRaw("ABSEnQ", _configurationRegisters.ABSEnQ);
    success &= _regAccess.writeRegisterRaw("CF2varh", _configurationRegisters.CF2varh);
    success &= _regAccess.writeRegisterRaw("3P3W", _configurationRegisters._3P3W);
    success &= _regAccess.writeRegisterRaw("didtEn", _configurationRegisters.didtEn);
    success &= _regAccess.writeRegisterRaw("HPFoff", _configurationRegisters.HPFoff);
    success &= _regAccess.writeRegisterRaw("Freq60Hz", _configurationRegisters.Freq60Hz);
    success &= _regAccess.writeRegisterRaw("PGA_GAIN", _configurationRegisters.PGA_GAIN);
    success &= _regAccess.writeRegisterRaw("PStartTh", _configurationRegisters.PStartTh);
    success &= _regAccess.writeRegisterRaw("QStartTh", _configurationRegisters.QStartTh);
    success &= _regAccess.writeRegisterRaw("SStartTh", _configurationRegisters.SStartTh);
    success &= _regAccess.writeRegisterRaw("PPhaseTh", _configurationRegisters.PPhaseTh);
    success &= _regAccess.writeRegisterRaw("QPhaseTh", _configurationRegisters.QPhaseTh);
    success &= _regAccess.writeRegisterRaw("SPhaseTh", _configurationRegisters.SPhaseTh);

    if (success) {
        Serial.println("Applied Configuration Registers");
    } else {
        Serial.println("Failed to apply Configuration Registers");
    }

    // Apply Calibration Registers (energy calibration)
    success &= _regAccess.writeRegisterRaw("PoffsetA", _calibrationRegisters.PoffsetA);
    success &= _regAccess.writeRegisterRaw("QoffsetA", _calibrationRegisters.QoffsetA);
    success &= _regAccess.writeRegisterRaw("PoffsetB", _calibrationRegisters.PoffsetB);
    success &= _regAccess.writeRegisterRaw("QoffsetB", _calibrationRegisters.QoffsetB);
    success &= _regAccess.writeRegisterRaw("PoffsetC", _calibrationRegisters.PoffsetC);
    success &= _regAccess.writeRegisterRaw("QoffsetC", _calibrationRegisters.QoffsetC);
    success &= _regAccess.writeRegisterRaw("PQGainA", _calibrationRegisters.PQGainA);
    success &= _regAccess.writeRegisterRaw("PhiA_DelayCycles", _calibrationRegisters.PhiA);
    success &= _regAccess.writeRegisterRaw("PQGainB", _calibrationRegisters.PQGainB);
    success &= _regAccess.writeRegisterRaw("PhiB_DelayCycles", _calibrationRegisters.PhiB);
    success &= _regAccess.writeRegisterRaw("PQGainC", _calibrationRegisters.PQGainC);
    success &= _regAccess.writeRegisterRaw("PhiC_DelayCycles", _calibrationRegisters.PhiC);

    if (success) {
        Serial.println("Applied Calibration Registers");
    } else {
        Serial.println("Failed to apply Calibration Registers");
    }

    // Apply Fundamental Harmonic Calibration Registers
    success &= _regAccess.writeRegisterRaw("PoffsetAF", _fundamentalHarmonicCalibrationRegisters.PoffsetAF);
    success &= _regAccess.writeRegisterRaw("PoffsetBF", _fundamentalHarmonicCalibrationRegisters.PoffsetBF);
    success &= _regAccess.writeRegisterRaw("PoffsetCF", _fundamentalHarmonicCalibrationRegisters.PoffsetCF);
    success &= _regAccess.writeRegisterRaw("PGainAF", _fundamentalHarmonicCalibrationRegisters.PGainAF);
    success &= _regAccess.writeRegisterRaw("PGainBF", _fundamentalHarmonicCalibrationRegisters.PGainBF);
    success &= _regAccess.writeRegisterRaw("PGainCF", _fundamentalHarmonicCalibrationRegisters.PGainCF);

    if (success) {
        Serial.println("Applied Fundamental Harmonic Calibration Registers");
    } else {
        Serial.println("Failed to apply Fundamental Harmonic Calibration Registers");
    }




    Serial.print("Applying Voltage Gain: ");
    Serial.println(_measurementCalibrationRegisters.UgainA);

    // Apply Measurement Calibration Registers (RMS calibration)
    success &= _regAccess.writeRegisterRaw("UgainA", _measurementCalibrationRegisters.UgainA);
    success &= _regAccess.writeRegisterRaw("IgainA", _measurementCalibrationRegisters.IgainA);
    success &= _regAccess.writeRegisterRaw("UoffsetA", _measurementCalibrationRegisters.UoffsetA);
    success &= _regAccess.writeRegisterRaw("IoffsetA", _measurementCalibrationRegisters.IoffsetA);
    success &= _regAccess.writeRegisterRaw("UgainB", _measurementCalibrationRegisters.UgainB);
    success &= _regAccess.writeRegisterRaw("IgainB", _measurementCalibrationRegisters.IgainB);
    success &= _regAccess.writeRegisterRaw("UoffsetB", _measurementCalibrationRegisters.UoffsetB);
    success &= _regAccess.writeRegisterRaw("IoffsetB", _measurementCalibrationRegisters.IoffsetB);
    success &= _regAccess.writeRegisterRaw("UgainC", _measurementCalibrationRegisters.UgainC);
    success &= _regAccess.writeRegisterRaw("IgainC", _measurementCalibrationRegisters.IgainC);
    success &= _regAccess.writeRegisterRaw("UoffsetC", _measurementCalibrationRegisters.UoffsetC);
    success &= _regAccess.writeRegisterRaw("IoffsetC", _measurementCalibrationRegisters.IoffsetC);

    if (success) {
        Serial.println("Applied Measurement Calibration Registers");
    } else {
        Serial.println("Failed to apply Measurement Calibration Registers");
    }

    // Apply EMM Status Registers (interrupt enables)
    success &= _regAccess.writeRegisterRaw("CF4RevIntEN", _emmStatusRegisters.CF4RevIntEN);
    success &= _regAccess.writeRegisterRaw("CF3RevIntEN", _emmStatusRegisters.CF3RevIntEN);
    success &= _regAccess.writeRegisterRaw("CF2RevIntEN", _emmStatusRegisters.CF2RevIntEN);
    success &= _regAccess.writeRegisterRaw("CF1RevIntEN", _emmStatusRegisters.CF1RevIntEN);
    success &= _regAccess.writeRegisterRaw("TASNoloadIntEN", _emmStatusRegisters.TASNoloadIntEN);
    success &= _regAccess.writeRegisterRaw("TPNoloadIntEN", _emmStatusRegisters.TPNoloadIntEN);
    success &= _regAccess.writeRegisterRaw("TQNoloadIntEN", _emmStatusRegisters.TQNoloadIntEN);
    success &= _regAccess.writeRegisterRaw("INOv0IntEN", _emmStatusRegisters.INOv0IntEN);
    success &= _regAccess.writeRegisterRaw("IRevWnIntEN", _emmStatusRegisters.IRevWnIntEN);
    success &= _regAccess.writeRegisterRaw("URevWnIntEN", _emmStatusRegisters.URevWnIntEN);
    success &= _regAccess.writeRegisterRaw("OVPhaseCIntEN", _emmStatusRegisters.OVPhaseCIntEN);
    success &= _regAccess.writeRegisterRaw("OVPhaseBIntEN", _emmStatusRegisters.OVPhaseBIntEN);
    success &= _regAccess.writeRegisterRaw("OVPhaseAIntEN", _emmStatusRegisters.OVPhaseAIntEN);
    success &= _regAccess.writeRegisterRaw("OIPhaseCIntEN", _emmStatusRegisters.OIPhaseCIntEN);
    success &= _regAccess.writeRegisterRaw("OIPhaseBIntEN", _emmStatusRegisters.OIPhaseBIntEN);
    success &= _regAccess.writeRegisterRaw("OIPhaseAIntEN", _emmStatusRegisters.OIPhaseAIntEN);
    success &= _regAccess.writeRegisterRaw("PERegAPIntEn", _emmStatusRegisters.PERegAPIntEn);
    success &= _regAccess.writeRegisterRaw("PERegBPIntEn", _emmStatusRegisters.PERegBPIntEn);
    success &= _regAccess.writeRegisterRaw("PERegCPIntEn", _emmStatusRegisters.PERegCPIntEn);
    success &= _regAccess.writeRegisterRaw("PERegTPIntEn", _emmStatusRegisters.PERegTPIntEn);
    success &= _regAccess.writeRegisterRaw("QERegAPIntEn", _emmStatusRegisters.QERegAPIntEn);
    success &= _regAccess.writeRegisterRaw("QERegBPIntEn", _emmStatusRegisters.QERegBPIntEn);
    success &= _regAccess.writeRegisterRaw("QERegCPIntEn", _emmStatusRegisters.QERegCPIntEn);
    success &= _regAccess.writeRegisterRaw("QERgTPIntEn", _emmStatusRegisters.QERgTPIntEn);
    success &= _regAccess.writeRegisterRaw("PhaseLossCIntEn", _emmStatusRegisters.PhaseLossCIntEn);
    success &= _regAccess.writeRegisterRaw("PhaseLossBIntEn", _emmStatusRegisters.PhaseLossBIntEn);
    success &= _regAccess.writeRegisterRaw("PhaseLossAIntEn", _emmStatusRegisters.PhaseLossAIntEn);
    success &= _regAccess.writeRegisterRaw("FreqLoIntEn", _emmStatusRegisters.FreqLoIntEn);
    success &= _regAccess.writeRegisterRaw("SagPhaseCIntEn", _emmStatusRegisters.SagPhaseCIntEn);
    success &= _regAccess.writeRegisterRaw("SagPhaseBIntEn", _emmStatusRegisters.SagPhaseBIntEn);
    success &= _regAccess.writeRegisterRaw("SagPhaseAIntEn", _emmStatusRegisters.SagPhaseAIntEn);
    success &= _regAccess.writeRegisterRaw("FreqHiIntEn", _emmStatusRegisters.FreqHiIntEn);

    if (success) {
        Serial.println("Applied EMM Status Registers");
    } else {
        Serial.println("Failed to apply EMM Status Registers");
    }

    // Lock calibration registers
    success &= _regAccess.writeRegister("CfgRegAccEn", 0x0000);

    if (success) {
        Serial.println("All registers applied to chip successfully");
    } else {
        Serial.println("Failed to apply all registers to chip");
    }

    return success;
}

String SettingsManager::trim(const String& str) {
    int start = 0;
    int end = str.length() - 1;
    
    while (start < str.length() && isspace(str[start])) {
        start++;
    }
    
    while (end >= start && isspace(str[end])) {
        end--;
    }
    
    return str.substring(start, end + 1);
}