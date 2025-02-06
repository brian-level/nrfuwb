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
#include "ipcmsg_def.h"

/// external representation of a ble connection for use by
/// any other subsystem this interacts with
//
typedef const void *ble_conn_handle_t;

/// max number of concurrent BLE connections
//
#define BT_MAX_CONCURRENT               ( 2 )

#define BT_UUID_STARGAZER_VAL           ( 0xF00D )
#define BT_UUID_LEVEL_VAL               ( 0xFDBF )

#define BT_UUID_SERVICE                 BT_UUID_STARGAZER_VAL

// Base UUID - use the SIG UUID here since we're using our SIG-assigned 16-bit UUID for the service
#define APP_BLE_PROFILE_BASE_UUID                           { 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define APP_BLE_PROFILE_SERVICE_SHORT_UUID                  ( 0xFDBF )

#define BLEProfile_ConsoleCharacteristicShortID             ( 0x0002 )
#define BLEProfile_CommandCharacteristicShortID             ( 0x0010 )
#define BLEProfile_StatusCharacteristicShortID              ( 0x0011 )
#define BLEProfile_ResponseCharacteristicShortID            ( 0x0012 )
#define BLEProfile_DFUrxCharacteristicShortID               ( 0x0013 )
#define BLEProfile_LevelServiceChangedCharacteristicShortID ( 0x0015 )
#define BLEProfile_SettingsNameCharacteristicShortID        ( 0x0020 )
#define BLEProfile_SettingsValueCharacteristicShortID       ( 0x0021 )
#define BLEProfile_SidewalkCharacteristicShortID            ( 0x96C9 )
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
#define BLEProfile_RELtestCharacteristicShortID             ( 0xABCD )
#endif

#define RESPONSE_CHARACTERISTIC_UUID    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00
#define BT_UUID_RESPONSE   BT_UUID_DECLARE_128(RESPONSE_CHARACTERISTIC_UUID)

typedef enum
{
    BT_DATA_TYPE_NONE       = 0,
    BT_DATA_TYPE_SESSION    = 1,    ///< encrypted session data probably wrapped IPC
    BT_DATA_TYPE_OTA        = 2     ///< raw OTA IPC data
}
ble_data_type_t;

typedef const char *(*ble_sys_status_getter_t)(char *inBuffer, size_t inBufferSize);
typedef const int   (*ble_connect_callback_t)(ble_conn_handle_t in_conn, const uint16_t inMTU, const bool isConnected);
typedef const int   (*ble_rxdata_callback_t)(ble_conn_handle_t in_conn, const ble_data_type_t inDataType, const uint8_t *inData, const size_t inDataLen);
typedef const int   (*ble_mtu_update_callback_t)(ble_conn_handle_t in_conn, const uint16_t inMTU);
typedef int         (*ble_ipc_receiver_t)(ipc_msg_msg_t *inMsg);

// connection
//
int     BLEunlock(ble_conn_handle_t in_conn);
int     BLEdisconnect(ble_conn_handle_t in_conn);
bool    BLEisShellEnabled(void);
int     BLEslice(uint32_t *outDelay);
int     BLEinit(const char *in_device_name);

int     BLEwriteResponseCharacteristic(ble_conn_handle_t in_conn, const uint8_t *const inData, const size_t inDataLen);
int     BLEsendIPCmessage(const ipc_msg_msg_t *inSubMsg);
int     BLEsetIPCrxCallback(ble_ipc_receiver_t inRx);
int     BLEsetDelegate(
                        ble_connect_callback_t    inConnectCallback,
                        ble_rxdata_callback_t     inRxDataCallback,
                        ble_mtu_update_callback_t inMTUupdateCallback
                        );

#if defined(CONFIG_BT) && defined(LEVEL_BT_SECURE)
SessionOwner_t* BLEGetCurrentSession(void);

void    BLESetCurrentSession(SessionOwner_t *inSession);
#endif

// advertising
//
int     BLEstartAdvertising(void);
int     BLEstopAdvertising(void);
int     BLEsetDeviceName(const char *inDeviceName);

