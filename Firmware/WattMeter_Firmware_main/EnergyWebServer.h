#ifndef ENERGYWEBSERVER_H
#define ENERGYWEBSERVER_H

#include <WebServer.h>
#include <ArduinoJson.h>
#include "RegisterAccess.h"
#include "SettingsManager.h"

// Forward declaration
class EnergyAccumulator;

class EnergyWebServer {
public:
    EnergyWebServer(RegisterAccess& regAccess, uint16_t port = 80);

    bool begin(const char* ssid, const char* password);
    bool reconnect();
    void handleClient();
    String getIPAddress();
    void setSettingsManager(SettingsManager* settings) { _settings = settings; }
    void setEnergyAccumulator(EnergyAccumulator* accumulator) { _energyAccumulator = accumulator; }
    bool settingsNeedReload();
    
private:
    RegisterAccess& _regAccess;
    SettingsManager* _settings;
    EnergyAccumulator* _energyAccumulator;
    WebServer _server;
    String _lastSSID;
    String _lastPassword;
    bool _settingsNeedReload;

    // Route handlers
    void handleRoot();
    void handleReadRegister();
    void handleWriteRegister();
    void handleWriteMultiple();
    void handleReadMultiple();
    void handleNotFound();
    void handleGetSettings();
    void handleSetSettings();
    void handleGetCalibration();
    void handleSetCalibration();
    void handleAutoCalibrate();
    void handleSaveSettings();
    void handleReloadSettings();
    void handleGetRegisters();
    void handleGetEnergy();
    void handleStartEnergyCalibration();
    void handleCompleteEnergyCalibration();
    
    // Helper functions
    void sendJSON(int code, JsonDocument& doc);
    void sendError(int code, const char* message);
};

#endif