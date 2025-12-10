#ifndef COMMANDPARSER_H
#define COMMANDPARSER_H

#include "RegisterAccess.h"
#include "EnergyWebServer.h"
#include "SDCardLogger.h"
#include "SettingsManager.h"
#include "RebootManager.h"



// Forward declarations
class EnergyWebServer;
class SDCardLogger;
class SettingsManager;
class RebootManager;


class CommandParser {
public:
    CommandParser(RegisterAccess& regAccess) 
        : _regAccess(regAccess), 
          _webServer(nullptr),
          _sdLogger(nullptr),
          _settings(nullptr),
          _rebootManager(nullptr) {}
    
    // Set references for various subsystems
    void setWebServer(EnergyWebServer* webServer) { _webServer = webServer; }
    void setSDLogger(SDCardLogger* logger) { _sdLogger = logger; }
    void setSettingsManager(SettingsManager* settings) { _settings = settings; }
    void setRebootManager(RebootManager* rebootManager) { _rebootManager = rebootManager; }
    
    void parseCommand(const String& cmdLine);
    void printHelp();
    
private:
    RegisterAccess& _regAccess;
    EnergyWebServer* _webServer;
    SDCardLogger* _sdLogger;
    SettingsManager* _settings;
    RebootManager* _rebootManager;
    
    void handleRead(const String& regName);
    void handleWrite(const String& regName, const String& valueStr);
    void handleIP();
    void handleWiFi(const String& args);
    void handleReconnect();
    
    // SD Card commands
    void handleSDStatus();
    void handleLogStart();
    void handleLogStop();
    void handleLogStatus();
    void handleLogInterval(const String& args);
    void handleBufferFlush();
    
    // Settings commands
    void handleSettingsLoad();
    void handleSettingsSave();
    void handleCalLoad();
    void handleCalSave();
    void handleCalApply();
    void handleCalRead();
    
    // System commands
    void handleReboot();
    void handleUptime();
    
    // Utility functions
    void toLowerCase(String& str);
    uint32_t parseValue(const String& valueStr, bool* success);
    void printRegisterValue(const char* regName, const RegisterDescriptor* reg, float value, uint32_t rawValue);
    void printError(const char* message);
};

#endif