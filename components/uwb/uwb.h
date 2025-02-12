
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int (*session_state_callback_t)(uint32_t session_id, uint8_t state, uint8_t reason);

typedef struct
{
    uint8_t mac_addr[8];
}
one_way_range_data_t;

typedef struct
{
    uint8_t mac_addr[8];
}
two_way_range_data_t;

// see FiRa consortium UCI Generic Specification
//
typedef struct
{
    uint8_t  sequence;
    uint32_t session_id;
    uint8_t  rcr_indication;
    uint16_t current_ranging_interval;
    uint8_t  ranging_measurement_type;
    uint8_t  antenna_pair_info;
    uint8_t  mac_addr_mode_indicator;
    uint8_t  reserved[8];
    uint8_t  number_of_measurements;

    union
    {
        one_way_range_data_t one_way_data;
        two_way_range_data_t two_way_data;
    }
    range_data;
}
range_data_t;

int UWBStart(uint32_t inSessionID);
int UWBStop(void);
bool UWBReady(void);
int UWBSlice(uint32_t *delay);
int UWBinit(session_state_callback_t inSesionStateCallback);

