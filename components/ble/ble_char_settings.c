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

#define COMPONENT_NAME ble_chrset
#include "Logging.h"

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <zephyr/settings/settings.h>

#include <stdlib.h>

#define BLEP_MAX_SETTING_NAME   (64)
#define BLEP_MAX_STRING_VALUE   (128)
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
#define REL_RW_SIZE             (16)
#endif

typedef enum
{
    ST_BOOL,
    ST_ENUM,
    ST_U8,
    ST_S8,
    ST_U16,
    ST_S16,
    ST_U32,
    ST_S32,
    ST_STRING,
    ST_BYTES
}
ble_setting_type_t;

typedef struct
{
    const struct bt_gatt_attr *setting_value_attr;
    uint8_t     name[BLEP_MAX_SETTING_NAME];
    uint8_t     path[2 * BLEP_MAX_SETTING_NAME];
    uint8_t     value[BLEP_MAX_STRING_VALUE];
    int32_t     valueByteSize;
    uint8_t     bytes[BLEP_MAX_STRING_VALUE / 2];
#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
    uint8_t     rel_data[BLEP_MAX_STRING_VALUE];
#endif
    ble_setting_type_t type;
    union
    {
        uint32_t u32v;
        int32_t  s32v;
        uint16_t u16v;
        int16_t  s16v;
        uint8_t  u8v;
        int8_t   s8v;
    }
                 numVal;
}
ble_setting_t;

static ble_setting_t mBLEsetting;

static int _PathAndTypeForSetting(const char *inSettingName, const char **outPath, ble_setting_type_t *outType)
{
    const char *path = "";
    int ret = -1;

    require(inSettingName, exit);
    require(outPath, exit);
    require(outType, exit);

    path = "THREAD_APP";
    ret = 0;

    if (!strcmp(inSettingName, "Channel") || !strcmp(inSettingName, "ZigbeeChannel"))
    {
        *outType = ST_U8;
    }
    else if (!strcmp(inSettingName, "PanId"))
    {
        *outType = ST_U16;
    }
    else if (!strcmp(inSettingName, "Prefix"))
    {
        *outType = ST_BYTES;
    }
    else if (!strcmp(inSettingName, "ExtPanId"))
    {
        *outType = ST_BYTES;
    }
    else if (!strcmp(inSettingName, "NetworkKey"))
    {
        *outType = ST_BYTES;
    }
    else if (!strcmp(inSettingName, "NetworkName"))
    {
        *outType = ST_STRING;
    }
    else if (!strcmp(inSettingName, "DevShortId"))
    {
        *outType = ST_STRING;
    }
    else if (!strcmp(inSettingName, "TxPower"))
    {
        *outType = ST_S32;
    }
    else if (!strcmp(inSettingName, "Tethered"))
    {
        *outType = ST_BOOL;
    }
    else if (!strcmp(inSettingName, "useTransport"))
    {
        *outType = ST_ENUM;
    }
    else
    {
        LOG_ERR("No such setting: %s", inSettingName);
        *outType = ST_U32;
        ret = -1;
    }

    *outPath = path;
exit:
    return ret;
}

static int _OnSettingLoaded(const char *inKey, size_t inLen, settings_read_cb inReadCallback, void *inCallbackArg, void *inParam)
{
    int ret = -1;

    require(inParam, exit);
    require(inReadCallback, exit);

    if (!strcmp(inKey, (char*)inParam))
    {
        // remember byte size in case we want to replace the value (writevalue)
        //
        mBLEsetting.valueByteSize = inLen;

        // format setting value into a string representation
        //
        switch (mBLEsetting.type)
        {
        case ST_BOOL:
            require(inLen <= sizeof(uint32_t), exit);
            switch (inLen) // be able to handle any size bool or enum
            {
            case 1:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u8v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u8v ? 1 : 0);
                break;
            case 2:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u16v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u16v ? 1 : 0);
                break;
            case 4:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u32v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u32v ? 1 : 0);
                break;
            default:
                LOG_ERR("can't happen");
                break;
            }
            break;

        case ST_ENUM:
            require(inLen <= sizeof(uint32_t), exit);
            switch (inLen) // be able to handle any size bool or enum
            {
            case 1:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u8v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u8v);
                break;
            case 2:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u16v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u16v);
                break;
            case 4:
                inReadCallback(inCallbackArg, &mBLEsetting.numVal.u32v, inLen);
                snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u32v);
                break;
            default:
                LOG_ERR("can't happen");
                break;
            }
            break;

        case ST_U8:
            require(inLen == sizeof(uint8_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.u8v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", (uint32_t)mBLEsetting.numVal.u8v);
            break;

        case ST_S8:
            require(inLen == sizeof(int8_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.s8v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%d", (int32_t)mBLEsetting.numVal.s8v);
            break;

        case ST_U16:
            require(inLen == sizeof(uint16_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.u16v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", (uint32_t)mBLEsetting.numVal.u16v);
            break;

        case ST_S16:
            require(inLen == sizeof(int16_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.s16v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%d", (int32_t)mBLEsetting.numVal.s16v);
            break;

        case ST_U32:
            require(inLen == sizeof(uint32_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.u32v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%u", mBLEsetting.numVal.u32v);
            break;

        case ST_S32:
            require(inLen == sizeof(int32_t), exit);
            inReadCallback(inCallbackArg, &mBLEsetting.numVal.s32v, inLen);
            snprintf(mBLEsetting.value, sizeof(mBLEsetting.value), "%d", mBLEsetting.numVal.s32v);
            break;

        case ST_STRING:
            if (inLen >= sizeof(mBLEsetting.value))
            {
                LOG_ERR("Setting string value overflow");
                inLen = sizeof(mBLEsetting.value) - 1;
                mBLEsetting.valueByteSize = inLen;
            }

            inReadCallback(inCallbackArg, mBLEsetting.value, inLen);
            mBLEsetting.value[inLen] = '\0';
            break;

        case ST_BYTES:
            if ((inLen * 2) >= sizeof(mBLEsetting.bytes))
            {
                LOG_ERR("Setting bytes value overflow");
                inLen = (sizeof(mBLEsetting.bytes) - 2) / 2;
                mBLEsetting.valueByteSize = inLen;
            }

            inReadCallback(inCallbackArg,mBLEsetting.bytes, inLen);

            // convert bytes to hex-ascii
            for (int i = 0; i < inLen; i++)
            {
                snprintf(mBLEsetting.value + 2 * i, sizeof(mBLEsetting.value) - 2 * i, "%02X", mBLEsetting.bytes[i]);
            }
            break;
        }

        LOG_DBG("value for name=%s= is %u bytes, type %u is %s", inKey, inLen, mBLEsetting.type, mBLEsetting.value);
    }

    ret = 0;

exit:
    return ret;
}

ssize_t BLEinternalWriteSettingName(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                           )
{
    const char *path;
    int ret = -1;
    ble_conn_context_t *bleconn;

    bleconn = BLEinternalConnectionOf(conn);
    require(bleconn, exit);

    mBLEsetting.value[0] = '\0';

    if (!bleconn->unlocked)
    {
        LOG_ERR("Write Name BLE is locked");
        BLEdisconnect(conn);
        len = 0;
    }

    if (len > 0 && buf != NULL)
    {
        if (len >= sizeof(mBLEsetting.name))
        {
            LOG_ERR("Setting name overflow");
            len = sizeof(mBLEsetting.name) - 1;
        }

        memcpy(mBLEsetting.name, buf, len);
        mBLEsetting.name[len] = '\0';

        LOG_DBG("Setting name=%s=", mBLEsetting.name);

        // get setting path for name, and type info
        //
        ret = _PathAndTypeForSetting(mBLEsetting.name, &path, &mBLEsetting.type);
        require_noerr(ret, exit);
        require(path, exit);

        // load the setting path, looking for setting name
        //
        mBLEsetting.valueByteSize = 0;
        ret = settings_load_subtree_direct(path, _OnSettingLoaded, mBLEsetting.name);
        require_noerr(ret, exit);

        if (mBLEsetting.valueByteSize == 0)
        {
            LOG_WRN("No value for %s", mBLEsetting.name);
            goto exit;
        }

        LOG_DBG("Got setting: %-20s value: %s", mBLEsetting.name, mBLEsetting.value);

        if (mBLEsetting.setting_value_attr)
        {
            struct bt_gatt_notify_params params;

            memset(&params, 0, sizeof(params));

            params.attr = mBLEsetting.setting_value_attr;
            params.data = mBLEsetting.value;
            params.len  = strlen(mBLEsetting.value);
            params.func = NULL;

            if (bt_gatt_is_subscribed(conn, params.attr, BT_GATT_CCC_NOTIFY))
            {
                ret = bt_gatt_notify_cb(conn, &params);
                LOG_INF("%d Notify: %-20s   value: %s", ret, mBLEsetting.name, mBLEsetting.value);
            }
        }
    }

exit:
    return 0;
}

ssize_t BLEinternalReadSettingName(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                           )
{
    ssize_t rval = 0;
    ble_conn_context_t *bleconn;

    bleconn = BLEinternalConnectionOf(conn);
    require(bleconn, exit);

    if(!bleconn->unlocked)
    {
        LOG_ERR("Read Name BLE is locked");
        BLEdisconnect(conn);
    }
    else
    {
        LOG_INF("BLE READ NAME REQUEST");
        rval = bt_gatt_attr_read(conn, attr, buf, len, offset, mBLEsetting.name, strlen(mBLEsetting.name));
    }

exit:
    return rval;
}

ssize_t BLEinternalWriteSettingValue(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                           )
{
    ble_conn_context_t *bleconn;

    bleconn = BLEinternalConnectionOf(conn);
    require(bleconn, exit);

    if(!bleconn->unlocked)
    {
        LOG_ERR("Write Val BLE is locked");
        BLEdisconnect(conn);
        len = 0;
    }

    if (len > 0 && buf != NULL)
    {
        const char *path;
        void *pValue = NULL;
        int ret = -1;

        if (len >= sizeof(mBLEsetting.value))
        {
            LOG_ERR("Setting value overflow");
            len = sizeof(mBLEsetting.value) - 1;
        }

        // get settings path for current name
        //
        require(mBLEsetting.name[0], exit);
        ret = _PathAndTypeForSetting(mBLEsetting.name, &path, &mBLEsetting.type);
        require_noerr(ret, exit);
        require(path, exit);

        // load the setting to get the actual byte-size of the setting since
        // we can't ever be sure what nrf uses for specific enums for example
        //
        mBLEsetting.valueByteSize = 0;
        ret = settings_load_subtree_direct(path, _OnSettingLoaded, mBLEsetting.name);
        require_noerr(ret, exit);

        if (mBLEsetting.valueByteSize == 0)
        {
            LOG_WRN("No value for %s", mBLEsetting.name);
            goto exit;
        }

        LOG_DBG("write value=%s= with %u bytes", mBLEsetting.name, len);
        LOG_DBG("prev value is %s", mBLEsetting.value);

        // put current value in path buffer so we can compare
        //
        strncpy(mBLEsetting.path, mBLEsetting.value, sizeof(mBLEsetting.path) - 1);
        mBLEsetting.path[sizeof(mBLEsetting.path) - 1] = '\0';

        // format string value into proper type (do this after reading the current
        // value which uses the value member)
        //
        memcpy(mBLEsetting.value, buf, len);
        mBLEsetting.value[len] = '\0';

        LOG_INF("Setting %s to %s", mBLEsetting.name, mBLEsetting.value);
        LOG_DBG("      - prev: %s", mBLEsetting.path);

        if (!strcmp(mBLEsetting.value, mBLEsetting.path))
        {
            LOG_WRN("Not setting %s to same value", mBLEsetting.name);
        }
        else
        {
            switch (mBLEsetting.type)
            {
            case ST_BOOL:
            case ST_ENUM:
                switch (mBLEsetting.valueByteSize) // be able to handle any size bool or enum
                {
                case 1:
                    mBLEsetting.numVal.u8v = (uint8_t)strtoul(mBLEsetting.value, NULL, 0);
                    pValue = &mBLEsetting.numVal.u8v;
                    break;
                case 2:
                    mBLEsetting.numVal.u16v = (uint16_t)strtoul(mBLEsetting.value, NULL, 0);
                    pValue = &mBLEsetting.numVal.u16v;
                    break;
                case 4:
                    mBLEsetting.numVal.u32v = (uint32_t)strtoul(mBLEsetting.value, NULL, 0);
                    pValue = &mBLEsetting.numVal.u32v;
                    break;
                default:
                    LOG_ERR("can't happen");
                    break;
                }
                break;

            case ST_U8:
                mBLEsetting.numVal.u8v = (uint8_t)strtoul(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.u8v;
                break;

            case ST_S8:
                mBLEsetting.numVal.s8v = (int8_t)strtol(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.s8v;
                break;

            case ST_U16:
                mBLEsetting.numVal.u16v = (uint16_t)strtoul(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.u16v;
                break;

            case ST_S16:
                mBLEsetting.numVal.s16v = (int16_t)strtol(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.s16v;
                break;

            case ST_U32:
                mBLEsetting.numVal.u32v = (uint32_t)strtoul(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.u32v;
                break;

            case ST_S32:
                mBLEsetting.numVal.s32v = (int32_t)strtol(mBLEsetting.value, NULL, 0);
                pValue = &mBLEsetting.numVal.s32v;
                break;

            case ST_STRING:
                pValue = mBLEsetting.value;
                mBLEsetting.valueByteSize = strlen((char*)pValue);
                break;

            case ST_BYTES:
                // convert hex-ascio to bytes
                {
                    int i;
                    int len;

                    len = strlen(mBLEsetting.value);
                    if (len != mBLEsetting.valueByteSize * 2)
                    {
                        LOG_ERR("Won't set byte field of %u with %u nibbles",
                                mBLEsetting.valueByteSize, len);
                        pValue = NULL;
                        break;
                    }

                    pValue = mBLEsetting.bytes;

                    for (i = 0; i < mBLEsetting.valueByteSize && pValue != NULL; i++)
                    {
                        uint8_t upper = mBLEsetting.value[2 * i];
                        uint8_t lower = mBLEsetting.value[(2 * i) + 1];

                        if (upper >= 'a' && upper <= 'f')
                        {
                            upper -= 'a';
                            upper += 10;
                        }
                        else if (upper >= 'A' && upper <= 'F')
                        {
                            upper -= 'A';
                            upper += 10;
                        }
                        else if (upper >= '0' && upper <= '9')
                        {
                            upper -= '0';
                        }
                        else
                        {
                            LOG_ERR("Bad hex digit in value %s", mBLEsetting.value);
                            pValue = NULL;
                            break;
                        }
                        if (lower >= 'a' && lower <= 'f')
                        {
                            lower -= 'a';
                            lower += 10;
                        }
                        else if (lower >= 'A' && lower <= 'F')
                        {
                            lower -= 'A';
                            lower += 10;
                        }
                        else if (lower >= '0' && lower <= '9')
                        {
                            lower -= '0';
                        }
                        else
                        {
                            LOG_ERR("Bad hex digit in value %s", mBLEsetting.value);
                            pValue = NULL;
                        }

                        mBLEsetting.bytes[i] = (upper << 4) | lower;
                    }
                }
                break;
            }
        }

        if (pValue)
        {
            // form full path to setting
            //
            snprintf(mBLEsetting.path, sizeof(mBLEsetting.path), "%s/%s", path, mBLEsetting.name);

            // and write the setting value
            //
            ret = settings_save_one(mBLEsetting.path, pValue, mBLEsetting.valueByteSize);
            require_noerr(ret, exit);

            LOG_DBG("NV set %s to val type %u len %u", mBLEsetting.path, mBLEsetting.type, mBLEsetting.valueByteSize);
        }
    }

exit:
    return 0;
}

ssize_t BLEinternalReadSettingValue(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                           )
{
    ssize_t rval = 0;
    ble_conn_context_t *bleconn;

    bleconn = BLEinternalConnectionOf(conn);
    require(bleconn, exit);

    if(!bleconn->unlocked)
    {
        LOG_ERR("Read Val BLE is locked");
        BLEdisconnect(conn);
    }
    else
    {
        rval = bt_gatt_attr_read(conn, attr, buf, len, offset, mBLEsetting.value, strlen(mBLEsetting.value));
    }

exit:
    return rval;
}

#if defined(CPU1_FACTORY_APP) || defined(CPU2_FACTORY_APP)
ssize_t BLEinternalReadSettingRelTest(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            void *buf,
                            uint16_t len,
                            uint16_t offset
                           )
{
    LOG_HEXDUMP_INF(mBLEsetting.rel_data, REL_RW_SIZE, "outgoing rel data buf");
    return bt_gatt_attr_read(conn, attr, buf, REL_RW_SIZE, offset, mBLEsetting.rel_data, sizeof(mBLEsetting.rel_data));
}

ssize_t BLEinternalWriteSettingRelTest(
                            struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf,
                            uint16_t len,
                            uint16_t offset,
                            uint8_t flags
                           )
{
    LOG_HEXDUMP_INF(buf, len, "incoming rel data buf");
    memset(mBLEsetting.rel_data, 0x00, sizeof(mBLEsetting.rel_data));
    memcpy(mBLEsetting.rel_data, buf, MIN(len, sizeof(mBLEsetting.rel_data)));
    return 0;
}
#endif

int BLECharSettingsInit(const struct bt_gatt_attr *in_setting_value_attr)
{
    mBLEsetting.setting_value_attr = in_setting_value_attr;
    return 0;
}

