#ifndef REGISTERDESCRIPTORS_H
#define REGISTERDESCRIPTORS_H

#include "RegisterTypes.h"
#include <cstddef>

// Forward declarations
float calculatePeakV(uint16_t raw);
float calculatePeakI(uint16_t raw);

// External declarations
extern const RegisterDescriptor registers[];
extern const uint16_t registerCount;


#endif // REGISTERDESCRIPTORS_H