#ifndef SDCARDLOGGER_H
#define SDCARDLOGGER_H

#include <SD.h>
#include "RegisterAccess.h"
#include "TimeManager.h"

// Forward declaration
class EnergyAccumulator;

// Structure to hold a single measurement

struct FieldValue {
    String name;
    float value;
    bool valid;
};

struct Measurement {
    FieldValue* fields;
    unsigned int fieldCount;
    time_t timestamp;
    double kWh;
};



class SDCardLogger {
public:
    SDCardLogger(RegisterAccess& regAccess, TimeManager& timeManager, int csPin, int cdPin = -1, int wpPin = -1);
    ~SDCardLogger();  // Destructor to free buffer
    
    // Initialization
    bool begin();
    bool isCardPresent();
    bool isWriteProtected();
    
    // Configuration
    void setBufferSize(unsigned int size);
    void setLoggingInterval(unsigned long intervalMs);
    void setPowerLossThreshold(float voltage);
    void enablePowerLossDetection(bool enable);
    void setEnergyAccumulator(EnergyAccumulator* accumulator) { _energyAccumulator = accumulator; }
    
    // Card detection and handling
    void checkCardStatus();
    bool mountCard();
    void unmountCard();
    
    // Data logging
    bool logMeasurement();
    void enableLogging(bool enable);
    bool isLoggingEnabled() { return _loggingEnabled; }
    bool setLogFields(const String& fieldList);
    String getLogFields();
    
    // Power loss handling
    void checkPowerStatus();
    bool handlePowerLoss();  // Emergency buffer flush
    bool handlePowerRestoration(); // Remount and resume logging
    bool isPowerLost() { return _powerLost; }
    bool isWaitingForPowerRestoration() { return _powerLost && !_initialized; }
    bool settingsNeedReload();
    
    // Must be called in loop()
    void update();
    
    // Manual operations
    String getCurrentLogPath(int year, int month, int day);
    
    // Statistics
    unsigned long getLogCount() { return _logCount; }
    unsigned long getLastLogTime() { return _lastLogTime; }
    unsigned int getBufferUsage() { return _bufferIndex; }
    unsigned int getBufferSize() { return _bufferSize; }
    
private:
    RegisterAccess& _regAccess;
    TimeManager& _timeManager;
    EnergyAccumulator* _energyAccumulator;
    int _csPin;
    int _cdPin;
    int _wpPin;

    bool _initialized;
    bool _cardPresent;
    bool _writeProtected;
    bool _loggingEnabled;
    bool _settingsNeedReload;
    
    // Buffering
    Measurement* _buffer;
    unsigned int _bufferSize;
    unsigned int _bufferIndex;
    
    // Power loss detection
    bool _powerLossDetectionEnabled;
    float _powerLossThreshold;
    bool _powerLost;
    unsigned long _lastPowerCheck;
    static const unsigned long POWER_CHECK_INTERVAL = 100;  // Check every 100ms

    // Field configuration
    String* _fieldNames;
    unsigned int _fieldCount;
    
    unsigned long _loggingInterval;
    unsigned long _lastLogTime;
    unsigned long _logCount;
    
    unsigned long _lastCardCheck;
    static const unsigned long CARD_CHECK_INTERVAL = 1000;
    
    // Helper functions
    bool allocateBuffer(unsigned int size);
    void freeBuffer();
    bool takeMeasurement(Measurement& m);
    bool flushBuffer();
    bool writeBufferToFile(Measurement* data, unsigned int count);
    bool ensureFolderStructure(int year, int month, int day);
    bool writeHeaderIfNeeded(const String& filepath);
    void printCardInfo();
    bool parseFieldList(const String& fieldList);
    void freeFieldNames();
    bool allocateMeasurementFields(Measurement& m);
    void freeMeasurementFields(Measurement& m);
    String generateCSVHeader();
    
};

#endif