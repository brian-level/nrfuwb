
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int (*session_state_callback_t)(uint32_t session_id, uint8_t state, uint8_t reason);

int UWBStart(
        const uint8_t inType,
        const uint32_t inSessionID,
        const uint8_t *inProfile,
        const int inProfileLength);
int UWBStop(void);
bool UWBReady(void);
int UWBSlice(uint32_t *delay);
int UWBinit(session_state_callback_t inSesionStateCallback);

