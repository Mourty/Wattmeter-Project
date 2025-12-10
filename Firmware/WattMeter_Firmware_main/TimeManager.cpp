#include "TimeManager.h"


TimeManager::TimeManager()
    : _tz(nullptr),
      _rtcValid(false),
      _hasBeenSynced(false),
      _lastSyncTime(0),
      _autoSyncInterval(86400),
      _lastSyncAttempt(0),
      _ntpServer("pool.ntp.org"),
      _calibrationOffset(0),
      _calibrationReferenceTime(0),
      _calibrationEnabled(true),
      _calibrationThreshold(5.0f),
      _minCalibrationDays(1) {
}

bool TimeManager::begin() {
    if (!_rtc.begin()) {
        Serial.println("ERROR: Couldn't find PCF8523 RTC");
        return false;
    }
    
    Serial.println("PCF8523 RTC found");
    
    // Check if RTC lost power and time is invalid
    if (_rtc.lostPower()) {
        Serial.println("WARNING: RTC lost power, time is invalid");
        _rtcValid = false;
        return true;  // RTC exists but needs time set
    }
    
    // Check if time is valid (after year 2020)
    DateTime now = _rtc.now();
    if (now.year() < 2020) {
        Serial.println("WARNING: RTC time appears invalid (before 2020)");
        _rtcValid = false;
        return true;
    }
    
    _rtcValid = true;
    Serial.print("RTC time is valid: ");
    Serial.println(getTimeString());
    
    return true;
}

time_t TimeManager::getUnixTime() {
    if (!_rtcValid) {
        Serial.println("WARNING: RTC time not valid, returning 0");
        return 0;
    }
    
    DateTime now = _rtc.now();
    return now.unixtime();
}

String TimeManager::getTimeString(const char* format) {
    if (!_rtcValid) {
        return "Invalid Time";
    }
    
    DateTime now = _rtc.now();
    char buffer[64];
    
    // YYYY-MM-DD HH:MM:SS
    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    
    return String(buffer);
}

void TimeManager::getDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) {
    if (!_rtcValid) {
        year = 1970; month = 1; day = 1;
        hour = 0; minute = 0; second = 0;
        return;
    }
    
    DateTime now = _rtc.now();
    year = now.year();
    month = now.month();
    day = now.day();
    hour = now.hour();
    minute = now.minute();
    second = now.second();
}

void TimeManager::setTimezone(TimeChangeRule dstRule, TimeChangeRule stdRule) {
    // Delete old timezone if it exists
    if (_tz != nullptr) {
        delete _tz;
    }
    
    // Create new timezone
    _tz = new Timezone(dstRule, stdRule);
    
    Serial.println("Timezone configured:");
    Serial.print("  DST: ");
    Serial.print(dstRule.abbrev);
    Serial.print(" UTC");
    Serial.println(dstRule.offset / 60);
    Serial.print("  STD: ");
    Serial.print(stdRule.abbrev);
    Serial.print(" UTC");
    Serial.println(stdRule.offset / 60);
}

bool TimeManager::syncFromNTP(const char* ntpServer) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: WiFi not connected, cannot sync from NTP");
        return false;
    }
    
    _ntpServer = String(ntpServer);
    
    Serial.println("Syncing time from NTP server...");
    Serial.print("NTP Server: ");
    Serial.println(ntpServer);
    
    // Get current RTC time before sync
    time_t rtcTimeBefore = 0;
    if (_rtcValid) {
        DateTime now = _rtc.now();
        rtcTimeBefore = now.unixtime();
    }
    
    // Simple NTP sync - UTC only
    configTime(0, 0, ntpServer);  // GMT offset 0, DST offset 0
    
    // Wait for time to be set (with timeout)
    int retries = 0;
    time_t ntpTime = 0;
    while (ntpTime < 100000 && retries < 10) {
        time(&ntpTime);
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println();
    
    if (ntpTime < 100000) {
        Serial.println("ERROR: Failed to obtain time from NTP");
        return false;
    }
    
    // Perform calibration if enabled
    if (_calibrationEnabled && _rtcValid && _calibrationReferenceTime > 0) {
        calculateAndApplyCalibration(ntpTime, rtcTimeBefore);
    }
    
    // Set PCF8523 with UTC time
    DateTime ntpTimeObj = DateTime(ntpTime);
    _rtc.adjust(ntpTimeObj);
    
    _rtcValid = true;
    _hasBeenSynced = true;
    _lastSyncTime = ntpTime;
    _lastSyncAttempt = millis();
    _calibrationReferenceTime = ntpTime;
    
    Serial.print("RTC synced with NTP (UTC): ");
    Serial.println(getTimeString());
    
    if (_tz != nullptr) {
        Serial.print("Local time: ");
        Serial.println(getLocalTimeString());
    }
    
    return true;
}


bool TimeManager::setTime(int year, int month, int day, int hour, int minute, int second) {
    DateTime newTime(year, month, day, hour, minute, second);
    _rtc.adjust(newTime);
    
    _rtcValid = true;
    _hasBeenSynced = true;  // Manual set counts as "synced"
    _lastSyncTime = newTime.unixtime();
    
    Serial.print("RTC manually set to: ");
    Serial.println(getTimeString());
    
    return true;
}

bool TimeManager::setTime(time_t unixTime) {
    DateTime newTime(unixTime);
    _rtc.adjust(newTime);
    
    _rtcValid = true;
    _hasBeenSynced = true;
    _lastSyncTime = unixTime;
    
    Serial.print("RTC set from Unix time: ");
    Serial.println(getTimeString());
    
    return true;
}

bool TimeManager::isRTCValid() {
    return _rtcValid;
}

bool TimeManager::hasBeenSynced() {
    return _hasBeenSynced;
}

time_t TimeManager::getLastSyncTime() {
    return _lastSyncTime;
}

void TimeManager::setAutoSyncInterval(unsigned long intervalSeconds) {
    _autoSyncInterval = intervalSeconds;
    Serial.print("Auto-sync interval set to ");
    Serial.print(intervalSeconds / 3600);
    Serial.println(" hours");
}

void TimeManager::update() {
    // Only try auto-sync if we have WiFi and enough time has passed
    if (!_rtcValid || !_hasBeenSynced) {
        return;  // Don't auto-sync if we've never been synced
    }
    
    unsigned long now = millis();
    unsigned long timeSinceLastSync = (now - _lastSyncAttempt) / 1000;  // Convert to seconds
    
    if (timeSinceLastSync >= _autoSyncInterval) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Auto-sync: Time to sync with NTP");
            syncFromNTP(_ntpServer.c_str());
        } else {
            Serial.println("Auto-sync: WiFi not connected, skipping");
            _lastSyncAttempt = now;  // Update attempt time to try again later
        }
    }
}



bool TimeManager::calculateAndApplyCalibration(time_t ntpTime, time_t rtcTime) {
    // Calculate drift
    double driftSeconds = (double)(rtcTime - ntpTime);
    
    Serial.print("RTC Drift detected: ");
    Serial.print(driftSeconds, 2);
    Serial.println(" seconds");
    
    // Check if drift exceeds threshold
    if (abs(driftSeconds) < _calibrationThreshold) {
        Serial.println("Drift within threshold, no calibration needed");
        return false;
    }
    
    // Calculate time elapsed since last calibration
    unsigned long secondsElapsed = rtcTime - _calibrationReferenceTime;
    unsigned long daysElapsed = secondsElapsed / 86400;
    
    // Check minimum days requirement
    if (daysElapsed < _minCalibrationDays) {
        Serial.print("Not enough time elapsed for accurate calibration (need at least ");
        Serial.print(_minCalibrationDays);
        Serial.print(" days, only ");
        Serial.print(daysElapsed);
        Serial.println(" days have passed)");
        return false;
    }
    
    Serial.print("Days since last calibration: ");
    Serial.println(daysElapsed);
    
    // Calculate new calibration offset
    int8_t newOffset = calculateCalibrationOffset(driftSeconds, daysElapsed);
    
    // Combine with existing offset
    int16_t combinedOffset = _calibrationOffset + newOffset;
    
    // Clamp to valid range (-64 to +63)
    if (combinedOffset > 63) combinedOffset = 63;
    if (combinedOffset < -64) combinedOffset = -64;
    
    _calibrationOffset = (int8_t)combinedOffset;
    
    Serial.print("New calibration offset: ");
    Serial.println(_calibrationOffset);
    
    // Apply the calibration
    return applyCalibrationOffset(_calibrationOffset);
}

int8_t TimeManager::calculateCalibrationOffset(double driftSeconds, unsigned long daysElapsed) {
    // PCF8523 calibration: each offset unit changes rate by ~4.34 ppm
    // This translates to ~0.375 seconds per day per offset unit
    
    // Calculate drift per day
    double driftPerDay = driftSeconds / (double)daysElapsed;
    
    // Calculate required offset (rounded to nearest integer)
    // Negative drift (clock too slow) needs positive offset
    int8_t offset = (int8_t)round(-driftPerDay / 0.375);
    
    Serial.print("Calculated offset adjustment: ");
    Serial.print(offset);
    Serial.print(" (drift per day: ");
    Serial.print(driftPerDay, 3);
    Serial.println(" seconds)");
    
    return offset;
}

bool TimeManager::applyCalibrationOffset(int8_t offset) {
    // Clamp to valid range
    if (offset > 63) offset = 63;
    if (offset < -64) offset = -64;
    
    // Apply calibration to PCF8523
    Pcf8523OffsetMode mode = PCF8523_TwoHours;
    _rtc.calibrate(mode, offset);
    
    Serial.print("Applied calibration offset to RTC: ");
    Serial.println(offset);
    
    return true;
}

void TimeManager::setCalibrationData(time_t referenceTime, int8_t offset) {
    _calibrationReferenceTime = referenceTime;
    _calibrationOffset = offset;
    
    Serial.print("Calibration data set - Reference time: ");
    Serial.print(referenceTime);
    Serial.print(", Offset: ");
    Serial.println(offset);
}

void TimeManager::getCalibrationData(time_t& referenceTime, int8_t& offset) {
    referenceTime = _calibrationReferenceTime;
    offset = _calibrationOffset;
}

String TimeManager::getLocalTimeString() {
    if (!_rtcValid) {
        return "Invalid Time";
    }
    
    if (_tz == nullptr) {
        return getTimeString() + " (no timezone set)";
    }
    
    time_t utc = getUnixTime();
    time_t local = _tz->toLocal(utc);
    
    struct tm* timeinfo = localtime(&local);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S%p", timeinfo);
    
    // Add timezone abbreviation
    String result = String(buffer);

    return result;
}

void TimeManager::getLocalDateTime(int& year, int& month, int& day, int& hour, int& minute, int& second) {
    if (!_rtcValid || _tz == nullptr) {
        getDateTime(year, month, day, hour, minute, second);
        return;
    }
    
    time_t utc = getUnixTime();
    time_t local = _tz->toLocal(utc);
    
    struct tm* timeinfo = localtime(&local);
    year = timeinfo->tm_year + 1900;
    month = timeinfo->tm_mon + 1;
    day = timeinfo->tm_mday;
    hour = timeinfo->tm_hour;
    minute = timeinfo->tm_min;
    second = timeinfo->tm_sec;
}