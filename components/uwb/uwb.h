
#pragma once

#include <stdint.h>
#include <stdbool.h>

int UWBRead(
                uint8_t *outData,
                int inSize,
                int *outCount);
int UWBWrite(
                const uint8_t *inData,
                const int inCount);

int UWBSlice(uint32_t *delay);
int UWBinit(void);

