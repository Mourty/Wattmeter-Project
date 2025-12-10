#ifndef REBOOTMANAGER_H
#define REBOOTMANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "TimeManager.h"
#include "SDCardLogger.h"

class RebootManager {
public:
    RebootManager(TimeManager& timeManager, SDCardLogger& sdLogger);
    
    void begin();
    void update();  // Call in loop()
    
    void setRebootInterval(unsigned long hours);
    void setRebootHour(int hour);  // -1 = any time, 0-23 = specific hour
    void enable(bool enabled);
    
    void scheduleReboot(unsigned long delayMs = 5000);  // Manual reboot
    unsigned long getUptimeSeconds();
    unsigned long getTimeSinceLastReboot();
    
private:
    TimeManager& _timeManager;
    SDCardLogger& _sdLogger;
    Preferences _prefs;
    
    bool _enabled;
    unsigned long _rebootIntervalMs;
    int _rebootHour;
    time_t _lastRebootTime;
    
    bool _rebootScheduled;
    unsigned long _rebootScheduledTime;
    
    void saveLastRebootTime();
    void loadLastRebootTime();
    bool shouldRebootNow();
    void performReboot();
};

#endif