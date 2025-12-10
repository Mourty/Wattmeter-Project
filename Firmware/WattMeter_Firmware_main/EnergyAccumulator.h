#ifndef ENERGYACCUMULATOR_H
#define ENERGYACCUMULATOR_H

#include <Arduino.h>
#include "RegisterAccess.h"

// Forward declaration
class SettingsManager;

// Calibration state for energy accumulation
struct EnergyCalibrationState {
    bool calibrating;           // Is calibration in progress?
    uint8_t phaseMask;         // Bitmask: bit 0=A, bit 1=B, bit 2=C
    unsigned long startTime;   // When calibration started
};

class EnergyAccumulator {
public:
    EnergyAccumulator(RegisterAccess& regAccess);

    // Initialization
    void begin(SettingsManager* settings);

    // Configuration
    void setReadInterval(unsigned long intervalMs);
    void setSaveInterval(unsigned long intervalMs);
    unsigned long getReadInterval() { return _readInterval; }
    unsigned long getSaveInterval() { return _saveInterval; }

    // Energy access (in kWh)
    double getAccumulatedEnergy(uint8_t phase);  // phase: 0=A, 1=B, 2=C
    void resetAccumulatedEnergy(uint8_t phase);
    void setAccumulatedEnergy(uint8_t phase, double kWh);

    // Must be called in loop()
    void update();

    // Calibration control
    bool startCalibration(uint8_t phaseMask);  // Bitmask: bit 0=A, bit 1=B, bit 2=C
    bool completeCalibration(uint8_t phase, float loadWatts, float durationMinutes);
    bool isCalibrating() { return _calibState.calibrating; }
    uint8_t getCalibratingPhases() { return _calibState.phaseMask; }

    // Force immediate save to SD card
    bool saveToSettings();

    // Get last read/save times for API
    unsigned long getLastReadTime() { return _lastReadTime; }
    unsigned long getLastSaveTime() { return _lastSaveTime; }

    // Get meter constant from settings
    uint16_t getMeterConstant();

private:
    RegisterAccess& _regAccess;
    SettingsManager* _settings;

    // Accumulated energy totals (kWh)
    double _accumulatedEnergy[3];  // Phase A, B, C

    // Timing
    unsigned long _readInterval;   // How often to read energy registers (ms)
    unsigned long _saveInterval;   // How often to save to SD card (ms)
    unsigned long _lastReadTime;   // Last time energy was read
    unsigned long _lastSaveTime;   // Last time data was saved

    // Calibration state
    EnergyCalibrationState _calibState;

    // Internal methods
    bool readEnergyRegister(uint8_t phase, float& wattHours);
    bool calculateAndApplyGain(uint8_t phase, float expectedWh, float measuredWh);
    const char* getPhaseRegisterName(uint8_t phase, bool isPQGain);
};

#endif
