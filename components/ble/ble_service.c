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

//const uint16_t kBLEProfile_ConsoleCharacteristicShortID             = BLEProfile_ConsoleCharacteristicShortID;
const uint16_t kBLEProfile_CommandCharacteristicShortID             = BLEProfile_CommandCharacteristicShortID;
const uint16_t kBLEProfile_StatusCharacteristicShortID              = BLEProfile_StatusCharacteristicShortID;
const uint16_t kBLEProfile_ResponseCharacteristicShortID            = BLEProfile_ResponseCharacteristicShortID;
const uint16_t kBLEProfile_DFUrxCharacteristicShortID               = BLEProfile_DFUrxCharacteristicShortID;
const uint16_t kBLEProfile_LevelServiceChangedCharacteristicShortID = BLEProfile_LevelServiceChangedCharacteristicShortID;
const uint16_t kBLEProfile_SettingsNameCharacteristicShortID        = BLEProfile_SettingsNameCharacteristicShortID;
const uint16_t kBLEProfile_SettingsValueCharacteristicShortID       = BLEProfile_SettingsValueCharacteristicShortID;
const uint16_t kBLEProfile_SidewalkCharacteristicShortID            = BLEProfile_SidewalkCharacteristicShortID;
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
const uint16_t kBLEProfile_RELtestCharacteristicShortID             = BLEProfile_RELtestCharacteristicShortID;
#endif

static struct bt_uuid_16 kCalthingsServiceID                = BT_UUID_INIT_16( APP_BLE_PROFILE_SERVICE_SHORT_UUID );
static struct bt_uuid_16 kCommandCharacteristicID           = BT_UUID_INIT_16( BLEProfile_CommandCharacteristicShortID );
static struct bt_uuid_16 kResponseCharacteristicID          = BT_UUID_INIT_16( BLEProfile_ResponseCharacteristicShortID );
static struct bt_uuid_16 kDFUrxCharacteristicID             = BT_UUID_INIT_16( BLEProfile_DFUrxCharacteristicShortID );
static struct bt_uuid_16 kStatusCharacteristicID            = BT_UUID_INIT_16( BLEProfile_StatusCharacteristicShortID );
static struct bt_uuid_16 kSettingsNameCharacteristicID      = BT_UUID_INIT_16( kBLEProfile_SettingsNameCharacteristicShortID );
static struct bt_uuid_16 kSettingsValueCharacteristicID     = BT_UUID_INIT_16( kBLEProfile_SettingsValueCharacteristicShortID );
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
static struct bt_uuid_16 kSettingsRELtestCharacteristicID   = BT_UUID_INIT_16( kBLEProfile_RELtestCharacteristicShortID );
#endif

static void _cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("notify changed");
}

BT_GATT_SERVICE_DEFINE(mLevelService,
    BT_GATT_PRIMARY_SERVICE(&kCalthingsServiceID),
    BT_GATT_CHARACTERISTIC(&kCommandCharacteristicID.uuid,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE,
        NULL, BLEinternalWriteCommand, NULL),
    BT_GATT_CHARACTERISTIC(&kResponseCharacteristicID.uuid,
        BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
        BLEinternalReadResponse, NULL, NULL),
    BT_GATT_CCC(_cccd_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&kDFUrxCharacteristicID.uuid,
        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP, BT_GATT_PERM_WRITE,
        NULL, BLEinternalWriteDFUrx, NULL),
    BT_GATT_CHARACTERISTIC(&kStatusCharacteristicID.uuid,
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ, BT_GATT_PERM_READ,
        BLEinternalReadStatus, NULL, NULL),
    BT_GATT_CHARACTERISTIC(&kSettingsNameCharacteristicID.uuid,
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        BLEinternalReadSettingName, BLEinternalWriteSettingName, NULL),
    BT_GATT_CHARACTERISTIC(&kSettingsValueCharacteristicID.uuid,
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        BLEinternalReadSettingValue, BLEinternalWriteSettingValue, NULL),
    BT_GATT_CCC(_cccd_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
    BT_GATT_CHARACTERISTIC(&kSettingsRELtestCharacteristicID.uuid,
        BT_GATT_CHRC_NOTIFY | BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        BLEinternalReadSettingRelTest, BLEinternalWriteSettingRelTest, NULL),
#endif
);

int BLEServiceInit(void)
{
    int ret;

    ret = BLECharSettingsInit(&mLevelService.attrs[2]);
    require_noerr(ret, exit);

    ret = BLECharCommandInit(&mLevelService.attrs[4]);
    require_noerr(ret, exit);
exit:
    return ret;
}

