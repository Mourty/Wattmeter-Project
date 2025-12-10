#ifndef REGISTERTYPES_H
#define REGISTERTYPES_H

#include <stdint.h>

// Access type
typedef enum {
    RW_READ,
    RW_WRITE,
    RW_READWRITE,
    RW_READWRITE1CLEAR,
    RW_READCLEAR
} RWType;

// Data type
typedef enum {
    DT_UINT8,
    DT_INT8,
    DT_UINT16,
    DT_INT16,
    DT_UINT32,
    DT_INT32,
    DT_BIT,
    DT_BITFIELD
} regType;

// Register descriptor structure
typedef struct {
    const char* friendlyName;
    const char* name;
    uint16_t address[2];   // Sometimes registers are two-deep (like 0x01, 0x00)
    uint8_t regCount;      // number of consecutive registers (1 = 16-bit, 2 = 32-bit)
    RWType rwType;         // read/write permission
    regType regType;      // data type
    uint8_t bitPos;        // start bit (for bitfields)
    uint8_t bitLen;        // bitfield length
    float scale;           // scaling factor (1.0 if none)
    float (*convertFunc)(uint16_t);  // optional conversion function name
    const char* unit;      // engineering units (e.g., "V", "A", "Hz")
} RegisterDescriptor;

#endif // REGISTERTYPES_H
