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

// define this 1 if board is mounted horizontally (long edge on table)
// or 0 of mounted vertically (short edge on table)
//
#define UWB_ORIENT_HORIZ    (1)

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

#ifdef CONFIG_SSD1306
#include "display.h"
static void _DisplayRange(float distance, float azimuth, float elevation)
{
    char text[64];
    int width;
    int xoff;
    int yoff;
    int i;
    int az_mag;
    int el_mag;

    DisplaySetFont(28);
    snprintf(text, sizeof(text), "%4.2f", distance);
        width = DisplayTextWidth(text);
    xoff = (DisplayWidth() - width + 1) / 2;
    yoff = 10;
    DisplayText(xoff, yoff, text);

    // do 0 to 8 < or > on top line to show azimuth
    DisplaySetFont(8);
    yoff = 0;
    xoff = DisplayWidth() - DisplayTextWidth(">>>>>>>>");
    az_mag = (int)(azimuth * 8.0 / 60.0);
    if (az_mag < 0)
    {
        az_mag = -az_mag;
    }
    if (az_mag > 8)
    {
        az_mag = 8;
    }
    else if (az_mag < 1)
    {
        az_mag = 1;
    }
    if (azimuth < 0)
    {
        for (i = 0; i < az_mag; i++)
        {
            text[i] = '<';
        }
        for (; i < 8; i++)
        {
            text[i] = ' ';
        }
        text[i] = '\0';
        DisplayText(0, 0, text);
        DisplayText(xoff, yoff, "        ");
    }
    else
    {
        for (i = 0; i < (8 - az_mag); i++)
        {
            text[i] = ' ';
        }
        for (; i < az_mag; i++)
        {
            text[i] = '>';
        }
        text[i] = '\0';
        DisplayText(xoff, 0, text);
        DisplayText(0, yoff, "        ");
    }

    // do 0 to 8 - for elevation
    el_mag = (int)(elevation * 8.0 / 60.0);
    if (el_mag < 0)
    {
        el_mag = -el_mag;
    }
    if (el_mag > 8)
    {
        el_mag = 8;
    }
    else if (el_mag < 1)
    {
        el_mag = 1;
    }
    if (elevation < 0)
    {
        yoff = 0;
        xoff = 0;

        for (i = 0; i < 8; i++)
        {
            DisplayText(xoff, yoff, (i <= el_mag) ? "-" : " ");
            yoff += 8;
        }
    }
    else
    {
        yoff = DisplayHeight() / 2;
        xoff = 0;

        for (i = 0; i < 8; i++)
        {
            yoff -= 8;
            DisplayText(xoff, yoff, (i <= el_mag) ? "+" : " ");
        }
    }

    snprintf(text, sizeof(text), "%7.3f  az %6.1f el %6.1f  am=%d em=%d",
           distance, azimuth, elevation, az_mag, el_mag);
    LOG_INF("%s", text);
}
#else
static void _DisplayRange(float distance, float azimuth, float elevation)
{
    char text[64];

    snprintf(text, sizeof(text), "%7.3f  %6.1f %6.1f",
           distance, azimuth, elevation);
    LOG_INF("%s", text);
#endif

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
    /* reserved = */                  _UWB_GET_UINT8(&cursor);
    range.mac_addr_mode_indicator   = _UWB_GET_UINT8(&cursor);
    for (i = 0; i < 8; i++)
    {
        _UWB_GET_UINT8(&cursor); // reserved
    }

    range.number_of_measurements    = _UWB_GET_UINT8(&cursor);

    LOG_DBG("Range %02u %08X type=%02u, num=%u",
                range.sequence, range.session_id, range.ranging_measurement_type,
                range.number_of_measurements);

    if (range.ranging_measurement_type == UWB_RANGE_MEASUREMENT_TYPE_ONE_WAY)
    {
        // TODO
        LOG_INF("Not handling 1-way data yet");
        ret = -EOPNOTSUPP;
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
                // TODO - decode the actual code? in practice is us usually
                // 0x21, 0x81, or 0x82
                ret = -ENETDOWN;
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

#if UWB_ORIENT_HORIZ
            _DisplayRange(distance, elevation, azimuth);
#else
            _DisplayRange(distance, azimuth, elevation);
#endif
        }
    }

    ret = 0;
exit:
    return ret;
}

