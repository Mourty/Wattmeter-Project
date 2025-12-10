#include "RegisterAccess.h"
#include <string.h>


const RegisterDescriptor* RegisterAccess::findRegister(const char* name) {
    for (size_t i = 0; i < registerCount; ++i) {
        if (strcmp(registers[i].name, name) == 0) {
            return &registers[i];
        }
    }
    return nullptr;
}


const RegisterDescriptor* RegisterAccess::getRegisterInfo(const char* name) {
    return findRegister(name);
}

float RegisterAccess::convertRegisterValue(const char* name, uint32_t rawValue) {
    const RegisterDescriptor* reg = findRegister(name);

    if (!reg) {
        return 0.0f;
    }

    // Apply conversion function if present
    if (reg->convertFunc && reg->regCount == 1) {
        return reg->convertFunc(rawValue);
    }

    // Apply scaling
    float scaled = 0.0f;
    if (reg->regType == DT_INT16 || reg->regType == DT_INT32) {
        // Handle signed values
        if (reg->regCount == 1) {
            scaled = (int16_t)rawValue * reg->scale;
        } else {
            scaled = (int32_t)rawValue * reg->scale;
        }
    } else {
        scaled = rawValue * reg->scale;
    }

    return scaled;
}

float RegisterAccess::readRegister(const char* name, bool* success) {
    // Read raw value first
    bool readSuccess = false;
    uint32_t rawValue = readRegisterRaw(name, &readSuccess);

    if (success) *success = readSuccess;

    if (!readSuccess) {
        return 0.0f;
    }

    // Convert the raw value to scaled float
    return convertRegisterValue(name, rawValue);
}

bool RegisterAccess::writeRegister(const char* name, float value) {
    const RegisterDescriptor* reg = findRegister(name);
    
    if (!reg) return false;
    if (reg->rwType == RW_READ) return false;  // Read-only
    
    // Reverse scaling
    uint32_t rawValue = (uint32_t)(value / reg->scale);
    
    return writeValue(reg, rawValue);
}

uint32_t RegisterAccess::readRegisterRaw(const char* name, bool* success) {
    const RegisterDescriptor* reg = findRegister(name);
    
    if (!reg || reg->rwType == RW_WRITE) {
        if (success) *success = false;
        return 0;
    }
    
    if (success) *success = true;
    return readValue(reg);
}

bool RegisterAccess::writeRegisterRaw(const char* name, uint32_t value) {
    const RegisterDescriptor* reg = findRegister(name);
    
    if (!reg || reg->rwType == RW_READ) return false;
    
    return writeValue(reg, value);
}

uint32_t RegisterAccess::readValue(const RegisterDescriptor* reg) {
    uint16_t addr = reg->address[0];
    
    switch (reg->regType) {
        case DT_BIT:
            return _chip.readBit(addr, reg->bitPos) ? 1 : 0;
            
        case DT_BITFIELD:
            return _chip.readBitfield(addr, reg->bitPos, reg->bitLen);
            
        case DT_UINT8:
        case DT_INT8:
            return _chip.readBitfield(addr, reg->bitPos, 8);
            
        case DT_UINT16:
        case DT_INT16:
            return _chip.read16(addr);
            
        case DT_UINT32:
        case DT_INT32:
            return _chip.read32(reg->address[0], reg->address[1]);
            
        default:
            return 0;
    }
}

bool RegisterAccess::writeValue(const RegisterDescriptor* reg, uint32_t value) {
    uint16_t addr = reg->address[0];
    
    switch (reg->regType) {
        case DT_BIT:
            _chip.writeBit(addr, reg->bitPos, value != 0);
            return true;
            
        case DT_BITFIELD:
            _chip.writeBitfield(addr, reg->bitPos, reg->bitLen, value);
            return true;
            
        case DT_UINT8:
        case DT_INT8:
            _chip.writeBitfield(addr, reg->bitPos, 8, value);
            return true;
            
        case DT_UINT16:
        case DT_INT16:
            _chip.write16(addr, value);
            return true;
            
        case DT_UINT32:
        case DT_INT32:
            // Write high word first, then low word
            _chip.write16(reg->address[0], (value >> 16) & 0xFFFF);
            _chip.write16(reg->address[1], value & 0xFFFF);
            return true;
            
        default:
            return false;
    }
}