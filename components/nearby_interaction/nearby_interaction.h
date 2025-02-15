
#pragma once

#include <stdint.h>
#include <stdbool.h>

int NIbleConnectHandler(const void * const inConnectionHandle, const uint16_t inMTU, const bool isConnected);
int NIrxMessage(void *ble_conn_ctx, const uint8_t *inData, const uint32_t inCount);
int NIslice(uint32_t *delay);
int NIinit(void);

