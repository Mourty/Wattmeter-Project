#ifndef REGISTERACCESS_H
#define REGISTERACCESS_H

#include "ATM90E32.h"
#include "RegisterDescriptors.h"

class RegisterAccess {
public:
    RegisterAccess(ATM90E32& chip) : _chip(chip) {}
    
    // Read a register by name, returning scaled float value
    float readRegister(const char* name, bool* success = nullptr);

    // Convert a raw register value to scaled float (does not read from chip)
    float convertRegisterValue(const char* name, uint32_t rawValue);

    // Write a register by name (handles scaling automatically)
    bool writeRegister(const char* name, float value);
    
    // Read raw value without scaling
    uint32_t readRegisterRaw(const char* name, bool* success = nullptr);
    
    // Write raw value without scaling
    bool writeRegisterRaw(const char* name, uint32_t value);
    
    // Optional: Get register info for debugging/display
    const RegisterDescriptor* getRegisterInfo(const char* name);
    
private:
    ATM90E32& _chip;
    
    // Moved from RegistorTableUtils
    const RegisterDescriptor* findRegister(const char* name);
    
    // Helper to read based on descriptor
    uint32_t readValue(const RegisterDescriptor* reg);
    
    // Helper to write based on descriptor
    bool writeValue(const RegisterDescriptor* reg, uint32_t value);
};

#endif