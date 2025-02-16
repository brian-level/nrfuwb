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
#include "nearby_interaction.h"

#define COMPONENT_NAME ble_chruwb
#include "Logging.h"

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <stdlib.h>

typedef struct
{
    const struct bt_gatt_attr *response_attr;
}
ble_uwbchar_t;

static ble_uwbchar_t mBLEuwb;

ssize_t BLEinternalWriteUWB(
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

    if (len > 0 && buf != NULL)
    {
        LOG_WRN("Write %u to UWB", len);
        NIrxMessage(conn, (uint8_t*)buf, (uint32_t)len);
    }
exit:
    return 0;
}

ssize_t BLEinternalReadUWB(
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

exit:
    return rval;
}

static void _onNotificationComplete(struct bt_conn *conn, void *user_data)
{
    ble_conn_context_t *bleconn = (ble_conn_context_t *)user_data;

    require(bleconn, exit);
    k_sem_give(&bleconn->notify_sem);
exit:
    return;
}

int BLEinternalNotifyUWB(struct bt_conn *conn, void *inData, uint16_t inDataLen)
{
    const struct bt_gatt_attr *attr = mBLEuwb.response_attr;
    int ret = -1;
    ble_conn_context_t *bleconn;

    require(conn, exit);
    bleconn = BLEinternalConnectionOf(conn);
    require(bleconn, exit);

    if (attr)
    {
        struct bt_gatt_notify_params params =
        {
            .uuid   = BT_UUID_UWB_TX,
            .attr   = attr,
            .data   = inData,
            .len    = inDataLen,
            .user_data = bleconn,
            .func   = _onNotificationComplete
        };

        // Check whether notifications are enabled or not
        if(bt_gatt_is_subscribed(bleconn->conn, attr, BT_GATT_CCC_NOTIFY))
        {
            // block for a bit to make sure previous notification has finished
            //
            int result = k_sem_take(&bleconn->notify_sem, K_MSEC(500));

            if (result)
            {
                LOG_ERR("Notify callback not fired");
            }

            // Send the notification
            int err = bt_gatt_notify_cb(bleconn->conn, &params);
            if(err)
            {
                LOG_ERR("Error, unable to send notification. Error %d", err);
            }
            else
            {
                LOG_DBG("Notified central of %u resp", inDataLen);
                ret = inDataLen;
            }
        }
        else
        {
            LOG_WRN("Notification not enabled on the selected attribute");
        }
    }

exit:
    return ret;
}

int BLECharUWBInit(const struct bt_gatt_attr *in_attr)
{
    mBLEuwb.response_attr = in_attr;
    return 0;
}


