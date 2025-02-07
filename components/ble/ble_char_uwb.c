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

#define COMPONENT_NAME ble_chruwb
#include "Logging.h"

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <stdlib.h>

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

    LOG_WRN("Read %u %u from UWB", len, offset);

exit:
    return rval;
}

