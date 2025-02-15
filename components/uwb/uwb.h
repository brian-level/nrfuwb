
#pragma once
#include "uci_defs.h"
#include <stdint.h>
#include <stdbool.h>

typedef int (*session_state_callback_t)(uint32_t session_id, uint8_t state, uint8_t reason);

int UWBgetSessionState(uint32_t *outSessionID, eSESSION_STATUS_t *outState);
int UWBstart(
        const uint8_t inType,
        const uint32_t inSessionID,
        const uint8_t *inProfile,
        const int inProfileLength);
int UWBstop(void);
bool UWBready(void);
int UWBslice(uint32_t *delay);
int UWBinit(session_state_callback_t inSesionStateCallback);

