#include "SDCardLogger.h"
#include "EnergyAccumulator.h"

SDCardLogger::SDCardLogger(RegisterAccess& regAccess, TimeManager& timeManager, int csPin, int cdPin, int wpPin)
    : _regAccess(regAccess),
      _timeManager(timeManager),
      _energyAccumulator(nullptr),
      _csPin(csPin),
      _cdPin(cdPin),
      _wpPin(wpPin),
      _initialized(false),
      _cardPresent(false),
      _writeProtected(false),
      _loggingEnabled(false),
      _settingsNeedReload(false),
      _buffer(nullptr),
      _bufferSize(60),
      _bufferIndex(0),
      _powerLossDetectionEnabled(true),
      _powerLossThreshold(100.0),
      _powerLost(false),
      _lastPowerCheck(0),
      _loggingInterval(1000),
      _lastLogTime(0),
      _logCount(0),
      _lastCardCheck(0),
      _fieldNames(nullptr),
      _fieldCount(0) {
    
    // Set default fields
    setLogFields("UrmsA,IrmsA,PmeanA,SmeanA,QmeanA,Freq");
    
    // Allocate initial buffer
    allocateBuffer(_bufferSize);
}


SDCardLogger::~SDCardLogger() {
    freeBuffer();
    freeFieldNames();
}

bool SDCardLogger::allocateBuffer(unsigned int size) {
    // Clamp to safe range
    if (size < 1) size = 1;
    if (size > 1000) size = 1000;
    
    // Free existing buffer if any
    freeBuffer();
    
    // Allocate new buffer
    _buffer = new (std::nothrow) Measurement[size];
    
    if (_buffer == nullptr) {
        Serial.printf("ERROR: Failed to allocate buffer for %u measurements\n", size);
        _bufferSize = 0;
        _bufferIndex = 0;
        return false;
    }
    
    // Initialize all measurements
    for (unsigned int i = 0; i < size; i++) {
        _buffer[i].fields = nullptr;
        _buffer[i].fieldCount = 0;
        _buffer[i].timestamp = 0;
        _buffer[i].kWh = 0.0;
    }
    
    _bufferSize = size;
    _bufferIndex = 0;
    
    Serial.printf("Buffer allocated: %u measurements\n", size);
    
    return true;
}

void SDCardLogger::freeBuffer() {
    if (_buffer != nullptr) {
        // Free all measurement fields
        for (unsigned int i = 0; i < _bufferSize; i++) {
            freeMeasurementFields(_buffer[i]);
        }
        delete[] _buffer;
        _buffer = nullptr;
    }
    _bufferSize = 0;
    _bufferIndex = 0;
}


void SDCardLogger::setBufferSize(unsigned int size) {
    // Don't reallocate if logging is active
    if (_loggingEnabled && _bufferIndex > 0) {
        Serial.println("Cannot change buffer size while logging with buffered data");
        Serial.println("Flush buffer first or disable logging");
        return;
    }
    
    allocateBuffer(size);
}

bool SDCardLogger::setLogFields(const String& fieldList) {
    // Don't change fields while logging with buffered data
    if (_loggingEnabled && _bufferIndex > 0) {
        Serial.println("Cannot change log fields while logging with buffered data");
        return false;
    }
    
    return parseFieldList(fieldList);
}

String SDCardLogger::getLogFields() {
    String result = "";
    for (unsigned int i = 0; i < _fieldCount; i++) {
        if (i > 0) result += ",";
        result += _fieldNames[i];
    }
    return result;
}

bool SDCardLogger::parseFieldList(const String& fieldList) {
    // Free existing field names
    freeFieldNames();
    
    // Count fields
    _fieldCount = 1;
    for (unsigned int i = 0; i < fieldList.length(); i++) {
        if (fieldList.charAt(i) == ',') {
            _fieldCount++;
        }
    }
    
    // Allocate array
    _fieldNames = new (std::nothrow) String[_fieldCount];
    if (_fieldNames == nullptr) {
        Serial.println("ERROR: Failed to allocate field names array");
        _fieldCount = 0;
        return false;
    }
    
    // Parse field names
    unsigned int fieldIndex = 0;
    int startPos = 0;
    
    for (unsigned int i = 0; i <= fieldList.length(); i++) {
        if (i == fieldList.length() || fieldList.charAt(i) == ',') {
            String fieldName = fieldList.substring(startPos, i);
            fieldName.trim();
            
            // Validate field exists
            const RegisterDescriptor* reg = _regAccess.getRegisterInfo(fieldName.c_str());
            if (reg == nullptr) {
                Serial.print("WARNING: Field '");
                Serial.print(fieldName);
                Serial.println("' not found in register list");
            } else if (reg->rwType == RW_WRITE) {
                Serial.print("WARNING: Field '");
                Serial.print(fieldName);
                Serial.println("' is write-only and cannot be logged");
            }
            
            _fieldNames[fieldIndex++] = fieldName;
            startPos = i + 1;
        }
    }
    
    Serial.printf("Log fields configured: %u fields\n", _fieldCount);
    Serial.print("Fields: ");
    Serial.println(getLogFields());
    
    return true;
}

void SDCardLogger::freeFieldNames() {
    if (_fieldNames != nullptr) {
        delete[] _fieldNames;
        _fieldNames = nullptr;
    }
    _fieldCount = 0;
}

bool SDCardLogger::allocateMeasurementFields(Measurement& m) {
    m.fields = new (std::nothrow) FieldValue[_fieldCount];
    if (m.fields == nullptr) {
        m.fieldCount = 0;
        return false;
    }
    m.fieldCount = _fieldCount;
    return true;
}

void SDCardLogger::freeMeasurementFields(Measurement& m) {
    if (m.fields != nullptr) {
        delete[] m.fields;
        m.fields = nullptr;
    }
    m.fieldCount = 0;
}

String SDCardLogger::generateCSVHeader() {
    String header = "";
    for (unsigned int i = 0; i < _fieldCount; i++) {
        if (i > 0) header += ",";

        // Get friendly name if available
        const RegisterDescriptor* reg = _regAccess.getRegisterInfo(_fieldNames[i].c_str());
        if (reg != nullptr && reg->friendlyName != nullptr && strlen(reg->friendlyName) > 0) {
            header += reg->friendlyName;
        } else {
            header += _fieldNames[i];
        }
    }
    header += ",kWh,UnixTime";
    return header;
}

void SDCardLogger::setLoggingInterval(unsigned long intervalMs) {
    _loggingInterval = intervalMs;
}

void SDCardLogger::setPowerLossThreshold(float voltage) {
    _powerLossThreshold = voltage;
}

void SDCardLogger::enablePowerLossDetection(bool enable) {
    _powerLossDetectionEnabled = enable;
}

bool SDCardLogger::begin() {
    Serial.println("\n=== SD Card Logger Initialization ===");
    
    // Setup card detect and write protect pins
    if (_cdPin >= 0) {
        pinMode(_cdPin, INPUT_PULLUP);
        Serial.print("Card Detect pin (GPIO");
        Serial.print(_cdPin);
        Serial.println(") configured");
    }
    
    if (_wpPin >= 0) {
        pinMode(_wpPin, INPUT_PULLUP);
        Serial.print("Write Protect pin (GPIO");
        Serial.print(_wpPin);
        Serial.println(") configured");
    }
    
    // Initial card status check
    checkCardStatus();
    
    // Try to mount if card is present
    if (_cardPresent) {
        return mountCard();
    } else {
        Serial.println("No SD card detected on startup");
        return false;
    }
}

void SDCardLogger::checkCardStatus() {
    bool previousCardPresent = _cardPresent;
    bool previousWriteProtected = _writeProtected;
    
    // Check card detect pin (active LOW - card present when pin is LOW)
    if (_cdPin >= 0) {
        _cardPresent = (digitalRead(_cdPin) == LOW);
    } else {
        // If no CD pin, assume card is present if already initialized
        _cardPresent = _initialized;
    }
    
    // Check write protect pin (active LOW - write protected when pin is HIGH)
    if (_wpPin >= 0 && _cardPresent) {
        _writeProtected = (digitalRead(_wpPin) == HIGH);
    } else {
        _writeProtected = false;
    }
    
    // Handle card insertion
    if (_cardPresent && !previousCardPresent) {
        Serial.println("\n*** SD Card inserted! ***");
        mountCard();
    }
    
    // Handle card removal
    if (!_cardPresent && previousCardPresent) {
        Serial.println("\n*** SD Card removed! ***");
        unmountCard();
    }
    
    // Handle write protect changes
    if (_cardPresent && _writeProtected != previousWriteProtected) {
        if (_writeProtected) {
            Serial.println("*** SD Card write-protected! ***");
            // Disable logging if it was enabled
            if (_loggingEnabled) {
                Serial.println("Data logging disabled due to write protection");
            }
        } else {
            Serial.println("*** SD Card write protection removed ***");
        }
    }
}

bool SDCardLogger::mountCard() {
    Serial.println("Attempting to mount SD card...");
    
    // Try multiple initialization methods for better compatibility
    
    // Method 1: Default initialization with slower clock
    Serial.println("Method 1: Trying default init with 400kHz clock...");
    if (SD.begin(_csPin, SPI, 400000)) {
        Serial.println("SD Card mounted successfully!");
        _initialized = true;
        printCardInfo();
        return true;
    }
    
    Serial.println("Method 1 failed");
    delay(100);
    
    // Method 2: Initialize SPI bus first
    Serial.println("Method 2: Initializing SPI bus first...");
    SPI.begin(18, 19, 23, _csPin);  // SCK, MISO, MOSI, CS
    delay(10);
    
    if (SD.begin(_csPin, SPI, 400000)) {
        Serial.println("SD Card mounted successfully!");
        _initialized = true;
        printCardInfo();
        return true;
    }
    
    Serial.println("Method 2 failed");
    delay(100);
    
    // Method 3: Try default clock speed
    Serial.println("Method 3: Trying default clock speed...");
    if (SD.begin(_csPin, SPI)) {
        Serial.println("SD Card mounted successfully!");
        _initialized = true;
        printCardInfo();
        return true;
    }
    
    Serial.println("Method 3 failed");
    Serial.println("=== SD Card Mount FAILED ===\n");
    
    _initialized = false;
    return false;
}

void SDCardLogger::unmountCard() {
    if (_initialized) {
        Serial.println("Unmounting SD card...");
        SD.end();
        _initialized = false;
        
        // Disable logging since card is gone
        if (_loggingEnabled) {
            _loggingEnabled = false;
            Serial.println("Data logging stopped (card removed)");
        }
    }
}

void SDCardLogger::checkPowerStatus() {
    if (!_powerLossDetectionEnabled) {
        return;
    }
    
    // Read voltage from energy IC
    bool success = false;
    float voltage = _regAccess.readRegister("UrmsA", &success);
    
    if (!success) {
        return;  // Can't check, skip this cycle
    }
    
    // Check if voltage dropped below threshold
    if (voltage < _powerLossThreshold && !_powerLost) {
        Serial.println("\n!!! POWER LOSS DETECTED !!!");
        Serial.printf("Voltage: %.2f V (threshold: %.2f V)\n", voltage, _powerLossThreshold);
        
        _powerLost = true;
        handlePowerLoss();
    }
    
    // Check if power restored
    if (voltage >= _powerLossThreshold && _powerLost) {
        Serial.println("\n*** Power restored ***");
        Serial.printf("Voltage: %.2f V\n", voltage);
        _powerLost = false;
        handlePowerRestoration();
    }
}

bool SDCardLogger::handlePowerRestoration() {
    Serial.println("Power restoration sequence initiated...");
    
    // Check if card is still present
    checkCardStatus();
    
    if (!_cardPresent) {
        Serial.println("WARNING: No SD card detected after power restoration");
        return false;
    }
    
    // Try to remount the card
    Serial.println("Attempting to remount SD card...");
    if (!mountCard()) {
        Serial.println("ERROR: Failed to remount SD card");
        return false;
    }
    
    // Check if write protected
    if (_writeProtected) {
        Serial.println("WARNING: SD card is write-protected, logging disabled");
        return false;
    }
    
    // Re-enable logging if everything is OK
    Serial.println("SD card remounted successfully");
    
    // Reset buffer to start fresh
    _bufferIndex = 0;
    
    // Set flag to indicate settings should be reloaded
    _settingsNeedReload = true;
    
    // Logging should resume automatically in update()
    Serial.println("Data logging will resume automatically");
    Serial.printf("Buffer size: %u measurements\n", _bufferSize);
    Serial.printf("Logging interval: %lu ms\n", _loggingInterval);
    
    return true;
}

bool SDCardLogger::settingsNeedReload() {
    bool result = _settingsNeedReload;
    _settingsNeedReload = false;  // Clear flag
    return result;
}

bool SDCardLogger::handlePowerLoss() {
    Serial.println("Emergency buffer flush initiated...");
    Serial.printf("Current buffer usage: %u / %u measurements\n", _bufferIndex, _bufferSize);

    // Save accumulated energy data before power loss
    if (_energyAccumulator) {
        Serial.println("Saving accumulated energy data...");
        if (_energyAccumulator->saveToSettings()) {
            Serial.println("Energy data saved successfully");
        } else {
            Serial.println("WARNING: Failed to save energy data!");
        }
    }

    // Take one final measurement if there's room
    if (_bufferIndex < _bufferSize && takeMeasurement(_buffer[_bufferIndex])) {
        _bufferIndex++;
        Serial.println("Final measurement captured");
    }

    // Flush whatever is in the buffer
    Serial.println("Flushing buffer to SD card...");
    bool success = flushBuffer();

    if (success) {
        Serial.printf("Emergency flush complete.\n");
    } else {
        Serial.println("ERROR: Emergency flush failed!");
    }

    // Unmount SD card safely
    Serial.println("Unmounting SD card for safety...");
    unmountCard();
    
    Serial.println("=================================");
    Serial.println("System in safe state");
    Serial.println("Waiting for power restoration...");
    Serial.println("=================================\n");
    
    return success;
}


bool SDCardLogger::isCardPresent() {
    return _cardPresent && _initialized;
}

bool SDCardLogger::isWriteProtected() {
    return _writeProtected;
}

void SDCardLogger::printCardInfo() {
    uint8_t cardType = SD.cardType();
    
    if (cardType == CARD_NONE) {
        Serial.println("WARNING: No SD card detected");
        _initialized = false;
        return;
    }
    
    Serial.print("SD Card Type: ");
    switch (cardType) {
        case CARD_MMC:
            Serial.println("MMC");
            break;
        case CARD_SD:
            Serial.println("SDSC");
            break;
        case CARD_SDHC:
            Serial.println("SDHC");
            break;
        default:
            Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %llu MB\n", cardSize);
    
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    Serial.printf("Used Space: %llu MB\n", usedBytes);
    
    uint64_t freeBytes = (SD.cardSize() - SD.usedBytes()) / (1024 * 1024);
    Serial.printf("Free Space: %llu MB\n", freeBytes);
    
    if (_writeProtected) {
        Serial.println("*** WRITE PROTECTED ***");
    } else {
        Serial.println("Write protection: OFF");
    }
    
    Serial.println("=====================================\n");
}


void SDCardLogger::enableLogging(bool enable) {
    if (enable && _writeProtected) {
        Serial.println("Cannot enable logging: SD card is write-protected");
        _loggingEnabled = false;
        return;
    }
    
    if (enable && !_cardPresent) {
        Serial.println("Cannot enable logging: No SD card present");
        _loggingEnabled = false;
        return;
    }
    
    if (enable && (_buffer == nullptr || _bufferSize == 0)) {
        Serial.println("Cannot enable logging: Buffer not allocated");
        _loggingEnabled = false;
        return;
    }
    
    // If disabling and buffer has data, flush it first
    if (!enable && _loggingEnabled && _bufferIndex > 0) {
        Serial.println("Flushing buffer before disabling logging...");
        flushBuffer();
    }
    
    _loggingEnabled = enable;
    if (enable) {
        Serial.println("Data logging enabled");
    } else {
        Serial.println("Data logging disabled");
    }
}

void SDCardLogger::update() {
    unsigned long now = millis();
    
    // Periodically check card status
    if (now - _lastCardCheck >= CARD_CHECK_INTERVAL) {
        checkCardStatus();
        _lastCardCheck = now;
    }
    
    // Check power status more frequently
    if (now - _lastPowerCheck >= POWER_CHECK_INTERVAL) {
        checkPowerStatus();
        _lastPowerCheck = now;
    }
    
    // Don't log if power is lost
    if (_powerLost) {
        return;
    }
    
    // Only log if everything is ready
    if (!_initialized || !_loggingEnabled || _writeProtected || !_timeManager.isRTCValid()) {
        return;
    }
    
    // Check if it's time to log
    if (now - _lastLogTime >= _loggingInterval) {
        if (logMeasurement()) {
            _lastLogTime = now;
        }
    }
}


bool SDCardLogger::takeMeasurement(Measurement& m) {
    // Free existing fields if any (safety check)
    if (m.fields != nullptr) {
        freeMeasurementFields(m);
    }
    
    // Allocate field storage for this measurement
    if (!allocateMeasurementFields(m)) {
        Serial.println("ERROR: Failed to allocate measurement fields");
        return false;
    }
    
    // Read all configured fields
    for (unsigned int i = 0; i < _fieldCount; i++) {
        m.fields[i].name = _fieldNames[i];
        m.fields[i].valid = false;

        bool success = false;
        m.fields[i].value = _regAccess.readRegister(_fieldNames[i].c_str(), &success);
        m.fields[i].valid = success;

        if (!success) {
            Serial.print("WARNING: Failed to read field: ");
            Serial.println(_fieldNames[i]);
        }
    }

    m.timestamp = _timeManager.getUnixTime();

    // Capture current kWh value (Phase A)
    if (_energyAccumulator) {
        m.kWh = _energyAccumulator->getAccumulatedEnergy(0);
    } else {
        m.kWh = 0.0;
    }

    return true;
}


bool SDCardLogger::logMeasurement() {
    if (!_initialized || _writeProtected || !_timeManager.isRTCValid()) {
        return false;
    }
    
    // Check if buffer is allocated
    if (_buffer == nullptr || _bufferSize == 0) {
        Serial.println("ERROR: Buffer not allocated");
        return false;
    }
    
    // Take measurement and add to buffer
    if (!takeMeasurement(_buffer[_bufferIndex])) {
        return false;
    }
    
    _bufferIndex++;
    _logCount++;
    
    // If buffer is full, flush it
    if (_bufferIndex >= _bufferSize) {
        Serial.printf("Buffer full (%u measurements), flushing to SD card...\n", _bufferIndex);
        return flushBuffer();
    }
    
    return true;
}

bool SDCardLogger::flushBuffer() {
    if (_bufferIndex == 0) {
        return true;  // Nothing to flush
    }
    
    if (!_initialized || _writeProtected) {
        Serial.println("Cannot flush: SD card not ready or write protected");
        return false;
    }
    
    unsigned long startTime = millis();
    bool success = writeBufferToFile(_buffer, _bufferIndex);
    unsigned long duration = millis() - startTime;
    
    if (success) {
        Serial.printf("Flushed %u measurements to SD card in %lu ms\n", _bufferIndex, duration);
        
        for (unsigned int i = 0; i < _bufferIndex; i++) {
            freeMeasurementFields(_buffer[i]);
        }
        
        _bufferIndex = 0;  // Reset buffer
    } else {
        Serial.println("ERROR: Failed to flush buffer to SD card");
    }
    
    return success;
}

bool SDCardLogger::writeBufferToFile(Measurement* data, unsigned int count) {
    if (count == 0) {
        return true;
    }
    
    // Get date from first measurement
    time_t firstTime = data[0].timestamp;
    struct tm* timeinfo = localtime(&firstTime);
    int year = timeinfo->tm_year + 1900;
    int month = timeinfo->tm_mon + 1;
    int day = timeinfo->tm_mday;
    
    // Ensure folder structure exists
    if (!ensureFolderStructure(year, month, day)) {
        return false;
    }
    
    // Build file path
    String filepath = getCurrentLogPath(year, month, day);
    
    // Check if file exists, create with header if not
    if (!SD.exists(filepath)) {
        if (!writeHeaderIfNeeded(filepath)) {
            return false;
        }
    }
    
    // Open file for appending
    File file = SD.open(filepath, FILE_APPEND);
    if (!file) {
        Serial.printf("Failed to open %s for appending\n", filepath.c_str());
        return false;
    }
    
    // Write all measurements in buffer
    for (unsigned int i = 0; i < count; i++) {
        // Check if we need to switch to a new file (date changed)
        struct tm* currentTime = localtime(&data[i].timestamp);
        int currentDay = currentTime->tm_mday;
        
        if (currentDay != day) {
            // Close current file and open new one for new day
            file.close();
            
            year = currentTime->tm_year + 1900;
            month = currentTime->tm_mon + 1;
            day = currentDay;
            
            if (!ensureFolderStructure(year, month, day)) {
                return false;
            }
            
            filepath = getCurrentLogPath(year, month, day);
            
            if (!SD.exists(filepath)) {
                if (!writeHeaderIfNeeded(filepath)) {
                    return false;
                }
            }
            
            file = SD.open(filepath, FILE_APPEND);
            if (!file) {
                return false;
            }
        }
        
        // Write CSV line - dynamic fields
        for (unsigned int j = 0; j < data[i].fieldCount; j++) {
            if (j > 0) file.print(",");

            if (data[i].fields[j].valid) {
                // Get register info for proper formatting
                const RegisterDescriptor* reg = _regAccess.getRegisterInfo(data[i].fields[j].name.c_str());

                if (reg != nullptr) {
                    // Format based on register type
                    if (reg->regType == DT_INT16 || reg->regType == DT_INT32) {
                        file.printf("%.0f", data[i].fields[j].value);  // Integer types
                    } else {
                        // Determine decimal places based on typical values
                        if (data[i].fields[j].name.indexOf("rms") >= 0) {
                            file.printf("%.3f", data[i].fields[j].value);  // Current (3 decimals)
                        } else {
                            file.printf("%.2f", data[i].fields[j].value);  // Default (2 decimals)
                        }
                    }
                } else {
                    file.printf("%.2f", data[i].fields[j].value);  // Default format
                }
            } else {
                file.print("NaN");  // Invalid reading
            }
        }

        // Write kWh (3 decimal places) from buffered measurement
        file.printf(",%.3f", data[i].kWh);

        // Write timestamp
        file.printf(",%ld\n", data[i].timestamp);
    }
    
    file.close();
    return true;
}


bool SDCardLogger::ensureFolderStructure(int year, int month, int day) {
    String basePath = "/data";
    if (!SD.exists(basePath)) {
        if (!SD.mkdir(basePath)) {
            Serial.println("Failed to create /data");
            return false;
        }
    }
    
    String yearPath = basePath + "/" + String(year);
    if (!SD.exists(yearPath)) {
        if (!SD.mkdir(yearPath)) {
            Serial.printf("Failed to create %s\n", yearPath.c_str());
            return false;
        }
    }
    
    char monthStr[3];
    sprintf(monthStr, "%02d", month);
    String monthPath = yearPath + "/" + String(monthStr);
    if (!SD.exists(monthPath)) {
        if (!SD.mkdir(monthPath)) {
            Serial.printf("Failed to create %s\n", monthPath.c_str());
            return false;
        }
    }
    
    return true;
}

String SDCardLogger::getCurrentLogPath(int year, int month, int day) {
    char path[64];
    sprintf(path, "/data/%04d/%02d/%02d.csv", year, month, day);
    return String(path);
}

bool SDCardLogger::writeHeaderIfNeeded(const String& filepath) {
    File file = SD.open(filepath, FILE_WRITE);
    if (!file) {
        Serial.printf("Failed to open %s for writing\n", filepath.c_str());
        return false;
    }
    
    // Write CSV header with configured fields
    file.println(generateCSVHeader());
    file.close();
    
    Serial.printf("Created new log file: %s\n", filepath.c_str());
    return true;
}

