#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <Arduino.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include <Timezone.h>

class TimeManager {
public:
RTC_PCF8523 _rtc;
    TimeManager();
    
    // Initialization
    bool begin();
    
    // Configure timezone (call after loading settings)
    void setTimezone(TimeChangeRule dstRule, TimeChangeRule stdRule);
    
    // Get current Unix timestamp from PCF8523 (always UTC)
    time_t getUnixTime();
    
    // Get formatted time strings
    String getTimeString(const char* format = "%Y-%m-%d %H:%M:%S");  // UTC
    String getLocalTimeString();  // Local time with timezone applied
    
    // Get date components (UTC)
    void getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second);
    
    // Get local date components
    void getLocalDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second);
    
    // NTP Sync (simplified - no timezone needed)
    bool syncFromNTP(const char* ntpServer = "pool.ntp.org");
    
    // Manual time set
    bool setTime(int year, int month, int day, int hour, int minute, int second);
    bool setTime(time_t unixTime);
    
    // Status checks
    bool isRTCValid();
    bool hasBeenSynced();
    time_t getLastSyncTime();
    
    // Periodic sync check
    void update();
    void setAutoSyncInterval(unsigned long intervalSeconds = 86400);
    
    // Calibration
    void setMinCalibrationDays(unsigned long days) { _minCalibrationDays = days; }
    void setCalibrationEnabled(bool enabled) { _calibrationEnabled = enabled; }
    void setCalibrationThreshold(float seconds) { _calibrationThreshold = seconds; }
    void setCalibrationData(time_t referenceTime, int8_t offset);
    void getCalibrationData(time_t& referenceTime, int8_t& offset);
    bool applyCalibrationOffset(int8_t offset);
    
private:
    
    Timezone* _tz;  
    bool _rtcValid;
    bool _hasBeenSynced;
    time_t _lastSyncTime;
    unsigned long _autoSyncInterval;
    unsigned long _lastSyncAttempt;
    
    // Calibration settings
    int8_t _calibrationOffset;
    time_t _calibrationReferenceTime;
    bool _calibrationEnabled;
    float _calibrationThreshold;
    unsigned long _minCalibrationDays;
    
    // NTP settings
    String _ntpServer;
    
    // Helper methods
    bool calculateAndApplyCalibration(time_t ntpTime, time_t rtcTime);
    int8_t calculateCalibrationOffset(double driftSeconds, unsigned long daysElapsed);
};

#endif