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
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

// meta-data on a single current ble connection
//
typedef struct
{
    struct bt_conn *conn;
    bool            unlocked;
    struct k_sem    notify_sem;
    bool            hasTxMsg;
}
ble_conn_context_t;

// context of entire ble system
//
typedef struct
{
    uint8_t     device_name[32];
    uint8_t     device_short[9];
    bool        has_advertised;
    bool        is_advertising;
    uint32_t    adv_stop_time;
    bool        shell_inited;

				ble_connect_callback_t connectCallback;
    ble_conn_context_t connections[BT_MAX_CONCURRENT];
}
ble_context_t;

extern ble_context_t mBLE;

// connection
//
ble_conn_context_t *BLEinternalConnectionOf(const void * const in_conn_handle);

// advertising
//
int BLEAdvertisingSlice(void);

// service
//
int BLEServiceInit(void);

// char_command
//
ssize_t BLEinternalWriteCommand(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                       );

ssize_t BLEinternalReadResponse(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                       );

ssize_t BLEinternalReadStatus(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                       );

ssize_t BLEinternalWriteDFUrx(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                       );

int BLECharCommandInit(const struct bt_gatt_attr *in_attr);

// char_settings
//
ssize_t BLEinternalWriteSettingName(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                       );

ssize_t BLEinternalWriteSettingValue(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                       );

ssize_t BLEinternalReadSettingName(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                       );

ssize_t BLEinternalReadSettingValue(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                       );

// UWB data

ssize_t BLEinternalWriteUWB(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                       );

ssize_t BLEinternalReadUWB(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                       );
int BLEinternalNotifyUWB(
                            struct bt_conn *conn,
                            void *buf,
                            uint16_t len
                       );
int BLECharSettingsInit(const struct bt_gatt_attr *in_attr);

int BLECharUWBInit(const struct bt_gatt_attr *in_attr);

