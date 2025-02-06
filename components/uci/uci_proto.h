
#pragma once

#include <stdint.h>
#include <stdbool.h>

int UCIprotoWrite(
                const uint8_t inType,
                const uint8_t inGID,
                const uint8_t inOID,
                const uint8_t *inData,
                const int inCount);
int UCIprotoRead(
                uint8_t *outType,
                uint8_t *outGID,
                uint8_t *outOID,
                uint8_t *outData,
                int inSize,
                int *outCount);
int UCIprotoSlice(uint32_t *delay);
int UCIprotoInit(bool inWarmStart);

