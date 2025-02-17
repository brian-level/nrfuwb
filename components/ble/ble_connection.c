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

#define COMPONENT_NAME ble_conn
#include "Logging.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <bluetooth/gatt_dm.h>
#include <zephyr/settings/settings.h>

#ifdef CONFIG_SHELL_BT_NUS
#include <shell/shell_bt_nus.h>
#endif

ble_context_t mBLE;

static void _connected(struct bt_conn *conn, uint8_t err)
{
    int conndex;
    bool dontconnect = false;

    if (err)
    {
        LOG_ERR("Connection failed (err %u)\n", err);
        return;
    }

    for (conndex = 0; conndex < BT_MAX_CONCURRENT; conndex++)
    {
        if (!mBLE.connections[conndex].conn)
        {
            if (mBLE.connectCallback)
            {
                uint16_t mtu = bt_gatt_get_mtu(conn);

                if (mBLE.connectCallback(conn, mtu, true))
                {
                    dontconnect = true;
                }
                else
                {
                    mBLE.connections[conndex].conn = bt_conn_ref(conn);
                }
            }
            else
            {
                mBLE.connections[conndex].conn = bt_conn_ref(conn);
            }

            break;
        }
    }

    if (dontconnect)
    {
        LOG_ERR("Conn-callback cancels conn\n");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    else if (conndex >= BT_MAX_CONCURRENT)
    {
        LOG_ERR("No available connection slots\n");
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    else
    {
        LOG_INF("Connected\n");
    }
}

static void _disconnected(struct bt_conn *conn, uint8_t reason)
{
    int conndex;

    LOG_INF("Disconnected (reason %u)\n", reason);

    // Unpair from all devices for now, pairing is required for
    // MDS but we may need multiple phones to access the service
    //
    bt_unpair(BT_ID_DEFAULT, NULL);

    for (conndex = 0; conndex < BT_MAX_CONCURRENT; conndex++)
    {
        if ((void*)mBLE.connections[conndex].conn == conn)
        {
            if (mBLE.connectCallback)
            {
                mBLE.connectCallback((void*)mBLE.connections[conndex].conn, 0, false);
            }

            bt_conn_unref(mBLE.connections[conndex].conn);
            mBLE.connections[conndex].conn = NULL;
            break;
        }
    }
}

static void _mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
    LOG_INF("Updated MTU conn %08X: TX: %d RX: %d bytes", (uint32_t)conn, tx, rx);

    /*
    if (mBLE.mtuUpdateCallback)
    {
        mBLE.mtuUpdateCallback(conn, ((rx < tx) ? rx : tx));
    }
    */
}

ble_conn_context_t *BLEinternalConnectionOf(const void * const in_conn_handle)
{
    int conndex;

    for (conndex = 0; conndex < BT_MAX_CONCURRENT; conndex++)
    {
        if (mBLE.connections[conndex].conn == in_conn_handle)
        {
            break;
        }
    }

    return (conndex < BT_MAX_CONCURRENT) ? &mBLE.connections[conndex] : NULL;
}

int BLEdisconnect(ble_conn_handle_t in_conn)
{
    int conndex;
    int result = -1;

    for (conndex = 0; conndex < BT_MAX_CONCURRENT; conndex++)
    {
        if ((void*)mBLE.connections[conndex].conn == in_conn)
        {
            // tell stack to disconnect
            // the disconnection callback will clear current conn
            //
            bt_conn_disconnect(mBLE.connections[conndex].conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            result = 0;
            break;
        }
    }

    return result;
}

BT_CONN_CB_DEFINE(conn_callbacks) =
{
    .connected          = _connected,
    .disconnected       = _disconnected,
};

static struct bt_gatt_cb gatt_callbacks = {
    .att_mtu_updated    = _mtu_updated
};

int BLEslice(uint32_t *outDelay)
{
    int result = 0;

    result = BLEAdvertisingSlice();
    return result;
}

int BLEinit(const char *in_device_name, ble_connect_callback_t inConnectCallback)
{
    int result = -1;

    memset(&mBLE, 0, sizeof(mBLE));

    for (int conndex = 0; conndex < BT_MAX_CONCURRENT; conndex++)
    {
        k_sem_init(&mBLE.connections[conndex].notify_sem, 1, 1);
    }

    result = BLEsetDeviceName(in_device_name);
    require_noerr(result, exit);

    result = bt_enable(NULL);
    require_noerr(result, exit);

    bt_gatt_cb_register(&gatt_callbacks);

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
        settings_load();
    }

    result = BLEServiceInit();
    require_noerr(result, exit);

    result = bt_set_name(mBLE.device_name);
    if (result)
    {
        LOG_ERR("Can't ble_set_name %s", mBLE.device_name);
    }

    mBLE.connectCallback = inConnectCallback;
exit:
    return result;
}

