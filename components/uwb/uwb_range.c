#include "uwb_range.h"
#include "uwb.h"
#include "uwb_defs.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME range
#include "Logging.h"

static uint8_t _UWB_GET_UINT8(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint8_t val = *cursor++;

    *pcursor = cursor;
    return val;
}

static uint16_t _UWB_GET_UINT16(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint16_t val = (uint16_t)*cursor++;

    val |= ((uint16_t)*cursor++) << 8;
    *pcursor = cursor;
    return val;
}

static uint8_t _UWB_GET_UINT32(uint8_t **pcursor)
{
    uint8_t *cursor = *pcursor;
    uint32_t val = (uint32_t)*cursor++;

    val |= ((uint32_t)*cursor++) << 8;
    val |= ((uint32_t)*cursor++) << 16;
    val |= ((uint32_t)*cursor++) << 24;
    *pcursor = cursor;
    return val;
}

int UWBrangeData(const uint8_t *inData, const int inCount)
{
    int ret = -EINVAL;
    range_data_t range;
    two_way_range_data_t two_way_data;
    uint8_t *cursor = (uint8_t *)inData;
    int remaining;
    int measurement;
    int i;

    float distance;
    float azimuth;
    float elevation;

    require(inData, exit);
    require(inCount >= 27, exit);

    range.sequence                  = _UWB_GET_UINT32(&cursor);
    range.session_id                = _UWB_GET_UINT32(&cursor);
    range.rcr_indication            = _UWB_GET_UINT8(&cursor);
    range.current_ranging_interval  = _UWB_GET_UINT32(&cursor);
    range.ranging_measurement_type  = _UWB_GET_UINT8(&cursor);
    _UWB_GET_UINT8(&cursor); // reserved
    range.mac_addr_mode_indicator   = _UWB_GET_UINT8(&cursor);
    for (i = 0; i < 8; i++)
    {
        _UWB_GET_UINT8(&cursor); // reserved
    }
    range.number_of_measurements    = _UWB_GET_UINT8(&cursor);

    LOG_INF("Range %02u %08X type=%02u, num=%u",
                range.sequence, range.session_id, range.ranging_measurement_type,
                range.number_of_measurements);

    if (range.ranging_measurement_type == UWB_RANGE_MEASUREMENT_TYPE_ONE_WAY)
    {
        // TODO
        LOG_INF("Not handling 1-way data yet");
        goto exit;
    }
    else
    {
        for (measurement = 0; measurement < range.number_of_measurements; measurement++)
        {
            remaining = inCount - (cursor - inData);
            memset(two_way_data.mac_addr, 0, sizeof(two_way_data.mac_addr));
            if (range.mac_addr_mode_indicator == UWB_MAC_MODE_2_BYTE)
            {
                require(remaining >= 18, exit);
                for (i = 0; i < 2; i++)
                {
                    two_way_data.mac_addr[i] = _UWB_GET_UINT8(&cursor);
                }
            }
            else
            {
                require(remaining >= 24, exit);
                for (i = 0; i < 8; i++)
                {
                    two_way_data.mac_addr[i] = _UWB_GET_UINT8(&cursor);
                }
            }

            two_way_data.status = _UWB_GET_UINT8(&cursor);
            if (two_way_data.status  != 0x00 && two_way_data.status  != 0x1B)
            {
                LOG_WRN("Range-error [%02X]", two_way_data.status);
                goto exit;
            }

            two_way_data.NLoS               = _UWB_GET_UINT8(&cursor);
            two_way_data.distance           = _UWB_GET_UINT16(&cursor);
            two_way_data.AoA_azimuth        = (int16_t)_UWB_GET_UINT16(&cursor);
            two_way_data.AoA_azimuth_fom    = _UWB_GET_UINT8(&cursor);
            two_way_data.AoA_elevation      = (int16_t)_UWB_GET_UINT16(&cursor);
            two_way_data.AoA_elevation_fom  = _UWB_GET_UINT8(&cursor);
            two_way_data.AoA_dst_azimuth        = (int16_t)_UWB_GET_UINT16(&cursor);
            two_way_data.AoA_dst_azimuth_fom    = _UWB_GET_UINT8(&cursor);
            two_way_data.AoA_dst_elevation      = (int16_t)_UWB_GET_UINT16(&cursor);
            two_way_data.AoA_dst_elevation_fom  = _UWB_GET_UINT8(&cursor);

            distance = (float)two_way_data.distance / 100.0;
            // angles are signed in 9.7 format
            azimuth = (float)(int)two_way_data.AoA_azimuth / (float)(1 << 7);
            elevation = (float)(int)two_way_data.AoA_elevation / (float)(1 << 7);

            LOG_INF("  M%d  Dist:%6.2f   Azim:%6.2f   Elev:%6.2f",
                   measurement + 1, distance, azimuth, elevation);
        }
    }
    ret = 0;
exit:
    return ret;
}

