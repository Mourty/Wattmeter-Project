#include "RebootManager.h"
#include <esp_system.h>

RebootManager::RebootManager(TimeManager& timeManager, SDCardLogger& sdLogger)
    : _timeManager(timeManager),
      _sdLogger(sdLogger),
      _enabled(true),
      _rebootIntervalMs(168UL * 3600UL * 1000UL),  // 1 week in ms
      _rebootHour(3),
      _lastRebootTime(0),
      _rebootScheduled(false),
      _rebootScheduledTime(0) {
}

void RebootManager::begin() {
    _prefs.begin("reboot", false);
    loadLastRebootTime();
    
    Serial.println("\n=== Reboot Manager ===");
    Serial.printf("Auto-reboot: %s\n", _enabled ? "Enabled" : "Disabled");
    Serial.printf("Interval: %lu hours\n", _rebootIntervalMs / (3600UL * 1000UL));
    Serial.printf("Preferred hour: %d\n", _rebootHour);
    
    if (_lastRebootTime > 0) {
        time_t now = _timeManager.getUnixTime();
        unsigned long hoursSince = (now - _lastRebootTime) / 3600;
        Serial.printf("Last reboot: %lu hours ago\n", hoursSince);
    } else {
        Serial.println("First boot (no reboot history)");
        saveLastRebootTime();  // Record this as first boot
    }
    
    Serial.println("=====================\n");
}

void RebootManager::update() {
    // Check for scheduled manual reboot
    if (_rebootScheduled && millis() >= _rebootScheduledTime) {
        performReboot();
    }
    
    // Check for automatic periodic reboot
    if (_enabled && shouldRebootNow()) {
        Serial.println("\nAutomatic reboot triggered");
        scheduleReboot(10000);  // 10 second warning
    }
}

void RebootManager::setRebootInterval(unsigned long hours) {
    _rebootIntervalMs = hours * 3600UL * 1000UL;
    Serial.printf("Reboot interval set to %lu hours\n", hours);
}

void RebootManager::setRebootHour(int hour) {
    if (hour < -1 || hour > 23) {
        Serial.println("Invalid reboot hour (must be -1 to 23)");
        return;
    }
    _rebootHour = hour;
}

void RebootManager::enable(bool enabled) {
    _enabled = enabled;
    Serial.printf("Auto-reboot %s\n", enabled ? "enabled" : "disabled");
}

bool RebootManager::shouldRebootNow() {
    if (!_timeManager.isRTCValid()) {
        return false;  // Don't reboot without valid time
    }
    
    time_t now = _timeManager.getUnixTime();
    time_t timeSinceReboot = now - _lastRebootTime;
    
    // Check if enough time has passed
    if (timeSinceReboot < (_rebootIntervalMs / 1000)) {
        return false;
    }
    
    // If specific hour is set, check if we're in that hour
    if (_rebootHour >= 0) {
        int year, month, day, hour, minute, second;
        _timeManager.getDateTime(year, month, day, hour, minute, second);
        
        if (hour != _rebootHour) {
            return false;  // Wait for the right hour
        }
    }
    
    return true;
}

void RebootManager::scheduleReboot(unsigned long delayMs) {
    if (_rebootScheduled) {
        return;  // Already scheduled
    }
    
    _rebootScheduled = true;
    _rebootScheduledTime = millis() + delayMs;
    
    Serial.println("\n=================================");
    Serial.printf("REBOOT SCHEDULED IN %lu SECONDS\n", delayMs / 1000);
    Serial.println("=================================\n");
}

void RebootManager::performReboot() {
    Serial.println("\n=================================");
    Serial.println("PERFORMING SYSTEM REBOOT");
    Serial.println("=================================\n");
    
    // Flush any buffered data
    if (_sdLogger.isLoggingEnabled() && _sdLogger.getBufferUsage() > 0) {
        Serial.println("Flushing data buffer...");
        // Buffer flush is handled internally by the logger
    }
    
    // Save reboot time for next boot
    saveLastRebootTime();
    
    delay(1000);  // Give serial time to flush
    
    // Reboot
    ESP.restart();
}

void RebootManager::saveLastRebootTime() {
    time_t now = _timeManager.getUnixTime();
    _prefs.putULong64("lastReboot", (uint64_t)now);
    _lastRebootTime = now;
    Serial.printf("Saved last reboot time: %ld\n", now);
}

void RebootManager::loadLastRebootTime() {
    _lastRebootTime = (time_t)_prefs.getULong64("lastReboot", 0);
}

unsigned long RebootManager::getUptimeSeconds() {
    return millis() / 1000;
}

unsigned long RebootManager::getTimeSinceLastReboot() {
    if (_lastRebootTime == 0 || !_timeManager.isRTCValid()) {
        return 0;
    }
    
    time_t now = _timeManager.getUnixTime();
    return now - _lastRebootTime;
}