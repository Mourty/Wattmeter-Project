#ifndef ATM90E32_H
#define ATM90E32_H

#include <Arduino.h>
#include <SPI.h>
#include "RegisterDescriptors.h"



class ATM90E32 {
public:
    // Constructor
    ATM90E32(int csPin);

    // Initialization
    bool begin();

    // Basic SPI read/write
    bool readBit(uint16_t addr, uint8_t pos);
    uint16_t readBitfield(uint16_t addr, uint8_t pos, uint8_t len);
    uint16_t read16(uint16_t addr);
    uint32_t read32(uint16_t addrHigh, uint16_t addrLow);
    
    void writeBit(uint16_t addr, uint8_t pos, bool value);
    void writeBitfield(uint16_t addr, uint8_t pos, uint8_t len, uint16_t value);
    void write16(uint16_t addr, uint16_t value);
    





    // Utility
    void setSPIClock(uint32_t hz);

private:
    int _csPin;
    uint32_t _spiClock;
    uint16_t spiTransfer16(uint16_t data);
};

#endif
