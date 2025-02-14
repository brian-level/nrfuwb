
#pragma once

#include <stdint.h>

#define UWB_RANGE_MEASUREMENT_TYPE_ONE_WAY  (0)
#define UWB_RANGE_MEASUREMENT_TYPE_TWO_WAY  (1)

#define UWB_MAC_MODE_2_BYTE                 (0)
#define UWB_MAC_MODE_8_BYTE                 (1)

typedef struct
{
    uint8_t  mac_addr[8];
    uint8_t  frame_type;
    uint8_t  NLoS;
    uint16_t distance;
    int16_t  AoA_azimuth;
    int8_t   AoA_azimuth_fom;
    int16_t  AoA_elevation;
    int8_t   AoA_elevation_fom;
    uint64_t timestamp;
    uint32_t blink_number;
    uint8_t  dev_specific_info_size;
    uint8_t  blink_payload_size;
}
one_way_range_data_t;

typedef struct
{
    uint8_t  mac_addr[8];
    uint8_t  status;
    uint8_t  NLoS;
    uint16_t distance;
    int16_t  AoA_azimuth;
    int8_t   AoA_azimuth_fom;
    int16_t  AoA_elevation;
    int8_t   AoA_elevation_fom;
    int16_t  AoA_dst_azimuth;
    int8_t   AoA_dst_azimuth_fom;
    int16_t  AoA_dst_elevation;
    int8_t   AoA_dst_elevation_fom;
    uint8_t  slot_index;
}
two_way_range_data_t;

// see FiRa consortium UCI Generic Specification
// modified by see NXP_SR150_UCI_Specification_v1.23
//
typedef struct
{
    uint32_t sequence;
    uint32_t session_id;
    uint8_t  rcr_indication;
    uint32_t current_ranging_interval;
    uint8_t  ranging_measurement_type;
    uint8_t  mac_addr_mode_indicator;
    uint8_t  number_of_measurements;
}
range_data_t;

int UWBrangeData(const uint8_t *inData, const int inCount);

