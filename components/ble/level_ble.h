/*
 * Copyright (c) 2024, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/// external representation of a ble connection for use by
/// any other subsystem this interacts with
//
typedef const void *ble_conn_handle_t;

/// max number of concurrent BLE connections
//
#define BT_MAX_CONCURRENT               ( 1 )

// The UWB service and characteristics
//
#define BT_UUID_SERIAL_PORT_SERVICE         0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
#define BT_UUID_SERIAL_PORT_SERVICE_SHORT   0x00, 0x01
#define BT_UUID_SERIAL_PORT_RX_CHAR         0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
#define BT_UUID_SERIAL_PORT_TX_CHAR         0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E

#define BT_UUID_SERVICE BT_UUID_DECLARE_128(BT_UUID_SERIAL_PORT_SERVICE)
#define BT_UUID_UWB_RX  BT_UUID_DECLARE_128(BT_UUID_SERIAL_PORT_RX_CHAR)
#define BT_UUID_UWB_TX  BT_UUID_DECLARE_128(BT_UUID_SERIAL_PORT_TX_CHAR)

typedef const int (*ble_connect_callback_t)(const void * const inConnectionHandle, const uint16_t inMTU, const bool isConnected);

// connection
//
int     BLEdisconnect(ble_conn_handle_t in_conn);
int     BLEslice(uint32_t *outDelay);
int     BLEinit(const char *in_device_name, ble_connect_callback_t inConnectCallback);

// advertising
//
int     BLEstartAdvertising(void);
int     BLEstopAdvertising(void);
int     BLEsetDeviceName(const char *inDeviceName);

int     BLEwriteUWBCharacteristic(ble_conn_handle_t in_conn, const uint8_t *const inData, const size_t inDataLen);

