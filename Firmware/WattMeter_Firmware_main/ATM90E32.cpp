*#include "ATM90E32.h"

ATM90E32::ATM90E32(int csPin) {
    _csPin = csPin;
    _spiClock = 100000; // default 100 kHz
}

bool ATM90E32::begin() {
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    return true;
}

void ATM90E32::setSPIClock(uint32_t hz) {
    _spiClock = hz;
}

// --- Single Bit Read ---
bool ATM90E32::readBit(uint16_t addr, uint8_t pos) {
    uint16_t regVal = read16(addr);
    return (regVal >> pos) & 0x01;
}

// --- Bitfield Read ---
uint16_t ATM90E32::readBitfield(uint16_t addr, uint8_t pos, uint8_t len) {
    uint16_t regVal = read16(addr);
    uint16_t mask = ((1 << len) - 1) << pos;
    return (regVal & mask) >> pos;
}

uint16_t ATM90E32::read16(uint16_t addr) {
  // Force MSB = 1 for read
  addr = (addr & 0x7FFF) | 0x8000;

  SPI.beginTransaction(SPISettings(_spiClock, MSBFIRST, SPI_MODE3));
  digitalWrite(_csPin, LOW);

  // Send the 16-bit address (two bytes)
  SPI.transfer16(addr);
  // Now clock out 16 bits of response
  uint16_t result = SPI.transfer16(0x0000);

  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();

  return result;
}

uint32_t ATM90E32::read32(uint16_t addrHigh, uint16_t addrLow) {
  uint32_t highWord = read16(addrHigh);
  uint32_t lowWord = read16(addrLow);
  uint32_t value = (highWord << 16) | lowWord;
  return value;
}

// --- Single Bit Write ---
void ATM90E32::writeBit(uint16_t addr, uint8_t pos, bool value) {
    uint16_t regVal = read16(addr);
    if (value)
        regVal |= (1 << pos);
    else
        regVal &= ~(1 << pos);
    write16(addr, regVal);
}


// --- Bitfield Write ---
void ATM90E32::writeBitfield(uint16_t addr, uint8_t pos, uint8_t len, uint16_t value) {
    uint16_t regVal = read16(addr);
    uint16_t mask = ((1 << len) - 1) << pos;
    regVal = (regVal & ~mask) | ((value << pos) & mask);
    write16(addr, regVal);
}

void ATM90E32::write16(uint16_t addr, uint16_t value) {
  // Clear MSB to indicate a write
   addr = addr & 0x7FFF;

  SPI.beginTransaction(SPISettings(_spiClock, MSBFIRST, SPI_MODE3));

  digitalWrite(_csPin, LOW);

  // Send 16-bit address (write command)
  SPI.transfer16(addr);

  // Send 16-bit data
  SPI.transfer16(value);

  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
}






    float calculatePeakV(uint16_t raw){
        float converted = 0;
        return converted;
    }
    float calculatePeakI(uint16_t raw){
        float converted = 0;
        return converted;
    }









