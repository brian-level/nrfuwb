/*
 * Copyright (c) 2022, Level Home Inc.
 *
 * All rights reserved.
 *
 * Proprietary and confidential. Unauthorized copying of this file,
 * via any medium is strictly prohibited.
 *
 */

#include "level_ble.h"
#include "ble_internal.h"

#define COMPONENT_NAME ble_service
#include "Logging.h"

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <zephyr/settings/settings.h>

#include <stdlib.h>

static void _cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("notify changed");
}

BT_GATT_SERVICE_DEFINE(mLevelService,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_SERVICE),
    BT_GATT_CHARACTERISTIC(BT_UUID_UWB_RX,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE,
        NULL, BLEinternalWriteUWB, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_UWB_TX,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
        BLEinternalReadUWB, NULL, NULL),
    BT_GATT_CCC(_cccd_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

int BLEServiceInit(void)
{
    int ret;

    ret = BLECharUWBInit(&mLevelService.attrs[3]);
    require_noerr(ret, exit);
exit:
    return ret;
}

