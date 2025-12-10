#include "EnergyAccumulator.h"
#include "SettingsManager.h"

EnergyAccumulator::EnergyAccumulator(RegisterAccess& regAccess)
    : _regAccess(regAccess), _settings(nullptr) {

    // Initialize accumulated energy
    _accumulatedEnergy[0] = 0.0;
    _accumulatedEnergy[1] = 0.0;
    _accumulatedEnergy[2] = 0.0;

    // Default intervals
    _readInterval = 20000;   // 20 seconds
    _saveInterval = 600000;  // 10 minutes

    // Initialize timing
    _lastReadTime = 0;
    _lastSaveTime = 0;

    // Not calibrating
    _calibState.calibrating = false;
    _calibState.phaseMask = 0;
    _calibState.startTime = 0;
}

void EnergyAccumulator::begin(SettingsManager* settings) {
    _settings = settings;

    if (_settings) {
        // Load settings from SettingsManager
        const EnergyAccumulationSettings& energySettings = _settings->getEnergyAccumulationSettings();

        _readInterval = energySettings.energyReadInterval;
        _saveInterval = energySettings.energySaveInterval;

        // Load accumulated energy from settings
        _accumulatedEnergy[0] = energySettings.accumulatedEnergyA;
        _accumulatedEnergy[1] = energySettings.accumulatedEnergyB;
        _accumulatedEnergy[2] = energySettings.accumulatedEnergyC;

        Serial.println("EnergyAccumulator: Loaded settings from SD card");
        Serial.printf("  Phase A: %.3f kWh\n", _accumulatedEnergy[0]);
        Serial.printf("  Phase B: %.3f kWh\n", _accumulatedEnergy[1]);
        Serial.printf("  Phase C: %.3f kWh\n", _accumulatedEnergy[2]);
        Serial.printf("  Read interval: %lu ms\n", _readInterval);
        Serial.printf("  Save interval: %lu ms\n", _saveInterval);
    }

    // Initialize timers
    _lastReadTime = millis();
    _lastSaveTime = millis();

    // Do an initial read to clear any accumulated energy in the chip
    Serial.println("EnergyAccumulator: Clearing energy registers");
    for (uint8_t phase = 0; phase < 3; phase++) {
        float dummy;
        readEnergyRegister(phase, dummy);
    }
}

void EnergyAccumulator::setReadInterval(unsigned long intervalMs) {
    _readInterval = intervalMs;
}

void EnergyAccumulator::setSaveInterval(unsigned long intervalMs) {
    _saveInterval = intervalMs;
}

double EnergyAccumulator::getAccumulatedEnergy(uint8_t phase) {
    if (phase < 3) {
        return _accumulatedEnergy[phase];
    }
    return 0.0;
}

void EnergyAccumulator::resetAccumulatedEnergy(uint8_t phase) {
    if (phase < 3) {
        _accumulatedEnergy[phase] = 0.0;
        saveToSettings();  // Immediately save reset
    }
}

void EnergyAccumulator::setAccumulatedEnergy(uint8_t phase, double kWh) {
    if (phase < 3) {
        _accumulatedEnergy[phase] = kWh;
    }
}

void EnergyAccumulator::update() {
    unsigned long now = millis();

    // Skip energy reading if we're in calibration mode
    if (!_calibState.calibrating) {
        // Check if it's time to read energy registers
        if (now - _lastReadTime >= _readInterval) {
            _lastReadTime = now;

            // Read energy from all three phases
            for (uint8_t phase = 0; phase < 3; phase++) {
                float wattHours = 0.0f;
                if (readEnergyRegister(phase, wattHours)) {
                    // Convert Wh to kWh and accumulate
                    _accumulatedEnergy[phase] += (wattHours / 1000.0);

                    if (wattHours > 0.01) {  // Only log if non-trivial amount
                        Serial.printf("Energy read - Phase %c: %.2f Wh (Total: %.3f kWh)\n",
                                    'A' + phase, wattHours, _accumulatedEnergy[phase]);
                    }
                } else {
                    Serial.printf("Warning: Failed to read energy register for phase %c\n", 'A' + phase);
                }
            }
        }
    }

    // Check if it's time to save to SD card
    if (now - _lastSaveTime >= _saveInterval) {
        _lastSaveTime = now;

        if (saveToSettings()) {
            Serial.println("EnergyAccumulator: Periodic save to SD card successful");
        } else {
            Serial.println("EnergyAccumulator: Warning - periodic save failed");
        }
    }
}

bool EnergyAccumulator::startCalibration(uint8_t phaseMask) {
    if (_calibState.calibrating) {
        Serial.println("EnergyAccumulator: Calibration already in progress");
        return false;
    }

    if (phaseMask == 0 || phaseMask > 0x07) {
        Serial.println("EnergyAccumulator: Invalid phase mask");
        return false;
    }

    Serial.printf("EnergyAccumulator: Starting calibration for phases:");
    if (phaseMask & 0x01) Serial.print(" A");
    if (phaseMask & 0x02) Serial.print(" B");
    if (phaseMask & 0x04) Serial.print(" C");
    Serial.println();

    // Unlock calibration registers
    _regAccess.writeRegister("CfgRegAccEn", 0x55AA);

    // Set PQGain to 0 (unity) for calibration phases and clear energy registers
    for (uint8_t phase = 0; phase < 3; phase++) {
        if (phaseMask & (1 << phase)) {
            const char* gainRegisterName = getPhaseRegisterName(phase, true);
            if (gainRegisterName) {
                _regAccess.writeRegisterRaw(gainRegisterName, 0);
                Serial.printf("  Set phase %c PQGain to 0 (unity)\n", 'A' + phase);
            }

            // Clear energy register
            float dummy;
            readEnergyRegister(phase, dummy);
            Serial.printf("  Cleared phase %c energy register\n", 'A' + phase);
        }
    }

    // Lock calibration registers
    _regAccess.writeRegister("CfgRegAccEn", 0x0000);

    // Enter calibration mode
    _calibState.calibrating = true;
    _calibState.phaseMask = phaseMask;
    _calibState.startTime = millis();

    return true;
}

bool EnergyAccumulator::completeCalibration(uint8_t phase, float loadWatts, float durationMinutes) {
    if (!_calibState.calibrating) {
        Serial.println("EnergyAccumulator: Not in calibration mode");
        return false;
    }

    if (phase >= 3) {
        Serial.println("EnergyAccumulator: Invalid phase");
        return false;
    }

    if ((_calibState.phaseMask & (1 << phase)) == 0) {
        Serial.printf("EnergyAccumulator: Phase %c not being calibrated\n", 'A' + phase);
        return false;
    }

    Serial.printf("EnergyAccumulator: Completing calibration for phase %c\n", 'A' + phase);
    Serial.printf("  Load: %.2f W, Duration: %.2f minutes\n", loadWatts, durationMinutes);

    // Calculate expected energy in Wh
    float expectedWh = loadWatts * (durationMinutes / 60.0);
    Serial.printf("  Expected energy: %.2f Wh\n", expectedWh);

    // Read measured energy from register
    float measuredWh = 0.0f;
    if (!readEnergyRegister(phase, measuredWh)) {
        Serial.println("  Error: Failed to read energy register");
        return false;
    }
    Serial.printf("  Measured energy: %.2f Wh\n", measuredWh);

    // Calculate and apply gain
    if (!calculateAndApplyGain(phase, expectedWh, measuredWh)) {
        Serial.println("  Error: Failed to calculate/apply gain");
        return false;
    }

    // Remove this phase from calibration mask
    _calibState.phaseMask &= ~(1 << phase);

    // If no more phases to calibrate, exit calibration mode
    if (_calibState.phaseMask == 0) {
        _calibState.calibrating = false;
        Serial.println("EnergyAccumulator: Calibration complete for all phases");

        // Resume normal operation - clear registers
        for (uint8_t p = 0; p < 3; p++) {
            float dummy;
            readEnergyRegister(p, dummy);
        }

        // Reset read timer
        _lastReadTime = millis();
    }

    return true;
}

bool EnergyAccumulator::saveToSettings() {
    if (!_settings) {
        return false;
    }

    // Get current settings
    EnergyAccumulationSettings energySettings = _settings->getEnergyAccumulationSettings();

    // Update accumulated energy values
    energySettings.accumulatedEnergyA = _accumulatedEnergy[0];
    energySettings.accumulatedEnergyB = _accumulatedEnergy[1];
    energySettings.accumulatedEnergyC = _accumulatedEnergy[2];
    energySettings.energyReadInterval = _readInterval;
    energySettings.energySaveInterval = _saveInterval;

    // Save back to settings manager
    _settings->setEnergyAccumulationSettings(energySettings);

    // Save to SD card
    return _settings->saveSettings();
}



bool EnergyAccumulator::readEnergyRegister(uint8_t phase, float& wattHours) {
    const char* registerName = getPhaseRegisterName(phase, false);
    if (!registerName) {
        return false;
    }

    bool success = false;
    float value = _regAccess.readRegister(registerName, &success);

    if (success) {
        // The register value is in units of 0.01 CF
        // Scale factor in descriptor is 0.01, so value is in CF units
        float cf = value;

        // Get meter constant
        uint16_t mc = getMeterConstant();

        // Convert CF to Wh: 1 CF = 1000 Wh / MC
        wattHours = cf * (1000.0f / mc);

        return true;
    }

    return false;
}

uint16_t EnergyAccumulator::getMeterConstant() {
    if (_settings) {
        return _settings->getEnergyAccumulationSettings().meterConstant;
    }
    return 3200;  // Default fallback
}

bool EnergyAccumulator::calculateAndApplyGain(uint8_t phase, float expectedWh, float measuredWh) {
    if (measuredWh < 0.1) {
        Serial.println("  Error: Measured energy too small (< 0.1 Wh)");
        return false;
    }

    // Calculate error: ε = (measured - expected) / expected
    float epsilon = (measuredWh - expectedWh) / expectedWh;
    Serial.printf("  Error (ε): %.6f (%.2f%%)\n", epsilon, epsilon * 100.0f);

    // Calculate gain using formula: Gain = Complementary((-ε/(1+ε)) × 2^15)
    float gainCalc = (-epsilon / (1.0f + epsilon)) * 32768.0f;
    int16_t newGain = (int16_t)gainCalc;  // Two's complement happens automatically

    Serial.printf("  Calculated gain: %.2f\n", gainCalc);
    Serial.printf("  New PQGain: 0x%04X (%d)\n", (uint16_t)newGain, newGain);

    const char* gainRegisterName = getPhaseRegisterName(phase, true);
    if (!gainRegisterName) {
        return false;
    }

    // Unlock calibration registers
    _regAccess.writeRegister("CfgRegAccEn", 0x55AA);

    // Write new gain to chip
    bool success = _regAccess.writeRegisterRaw(gainRegisterName, (uint16_t)newGain);

    // Lock calibration registers
    _regAccess.writeRegister("CfgRegAccEn", 0x0000);

    if (!success) {
        Serial.println("  Error: Failed to write new PQGain to chip");
        return false;
    }

    // Update settings
    if (_settings) {
        CalibrationRegisters calRegs = _settings->getCalibrationRegisters();

        if (phase == 0) calRegs.PQGainA = newGain;
        else if (phase == 1) calRegs.PQGainB = newGain;
        else if (phase == 2) calRegs.PQGainC = newGain;

        _settings->setCalibrationRegisters(calRegs);
        _settings->saveSettings();

        Serial.println("  Gain saved to settings.ini");
    }

    return true;
}

const char* EnergyAccumulator::getPhaseRegisterName(uint8_t phase, bool isPQGain) {
    if (isPQGain) {
        // PQGain registers
        switch (phase) {
            case 0: return "PQGainA";
            case 1: return "PQGainB";
            case 2: return "PQGainC";
            default: return nullptr;
        }
    } else {
        // APenergy registers
        switch (phase) {
            case 0: return "APenergyA";
            case 1: return "APenergyB";
            case 2: return "APenergyC";
            default: return nullptr;
        }
    }
}
