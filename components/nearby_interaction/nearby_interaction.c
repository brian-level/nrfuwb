
#include "nearby_interaction.h"
#include "uwb.h"
#include "uwb_defs.h"
#include "uci_defs.h"
#include "level_ble.h"
#include "ble_internal.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME ni
#include "Logging.h"

#define NI_VER_MAJ      (1)
#define NI_VER_MIN      (0)

#define NI_MAX_MESSAGE  256

static struct
{
    void        *conn_ctx;
    uint8_t     msgbuf[NI_MAX_MESSAGE];
    uint32_t    msgcnt;
    uint8_t     session_state;
}
mNI;

static void _NIputUINT16(uint8_t **pcursor, uint16_t data)
{
    uint8_t *cursor = *pcursor;

    *cursor++ = data & 0xFF;
    *cursor++ = data >> 8;
    *pcursor = cursor;
}

static void _NIputUINT8(uint8_t **pcursor, uint8_t data)
{
    uint8_t *cursor = *pcursor;

    *cursor++ = data;
    *pcursor = cursor;
}

static uint8_t canned_cd[] = { 0x01, 0x00, 0x01, 0x00, 0x32, 0x11, 0x10, 0x00, 0x46, 0x01,
                            0x31, 0x00, 0x00, 0x00, 0x06, 0x04, 0x01, 0x05, 0xda, 0x64, 0x00 };

static int _NIcreateIOSacd(void)
{
    // Nearby-Interaction-Accessory-Protocol-Specification-Release-R2-1.pdf

    uint8_t *cursor = mNI.msgbuf;
    int pad;

    _NIputUINT8(&cursor, UWBMSG_CONFIG_DATA);   // heasder cmd for dispatch

    _NIputUINT16(&cursor, NI_VER_MAJ);  // MajorVersion
    _NIputUINT16(&cursor, NI_VER_MIN);  // MinorVersion
    _NIputUINT8(&cursor, 20);           // PreferredUpdateRate

    for (pad = 0; pad < 10; pad++)
    {
        _NIputUINT8(&cursor, 0);        // Reserved
    }

    _NIputUINT8(&cursor, sizeof(canned_cd));   // UWBconfigDataLength

    for (pad = 0; pad < sizeof(canned_cd); pad++)
    {
        _NIputUINT8(&cursor, canned_cd[pad]);   // Reserved
    }

    mNI.msgcnt = cursor - mNI.msgbuf;
    return 0;
}

static int _NIcreateStarted(void)
{
    uint8_t *cursor = mNI.msgbuf;

    _NIputUINT8(&cursor, UWBMSG_DID_START);   // heasder cmd for dispatch
    mNI.msgcnt = cursor - mNI.msgbuf;
    return 0;
}

static int _NIcreateStopped(void)
{
    uint8_t *cursor = mNI.msgbuf;

    _NIputUINT8(&cursor, UWBMSG_DID_START);   // heasder cmd for dispatch
    mNI.msgcnt = cursor - mNI.msgbuf;
    return 0;
}

static int _NItxMessage(const uint8_t *inData, const uint32_t inCount)
{
    int ret;

    if (mNI.conn_ctx)
    {
        // TODO - what if conn_ctx is an int handle and 0 is valid?
        // maybe use a .is_conn_ctx_set member?
        //
        ret = BLEinternalNotifyUWB(mNI.conn_ctx, (void*)inData, inCount);
    }
    else
    {
        LOG_INF("No BLE connection to notify");
        ret = 0;
    }
    return ret;
}

int NIrxMessage(void *conn_ctx, const uint8_t *inData, const uint32_t inCount)
{
    int ret = -EINVAL;
    uint32_t sessionID;

    require(inData, exit);
    require(inCount > 0, exit);

    mNI.conn_ctx = conn_ctx;

    LOG_INF("Got Message %02X %u bytes", inData[0], inCount);

    sessionID = 0x11223344;

    switch (inData[0])
    {
    case UWBMSG_INITIALIZE_IOS:
        ret = UWBStart(sessionID);
        break;
    case UWBMSG_INITIALIZE_ANDROID:
        LOG_WRN("Android not supported yet");
        break;
    case UWBMSG_CONFIG_AND_START:
        //ret = UWBStart();
        ret = 0;
        if (!ret)
        {
            ret = _NIcreateStarted();
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        ret = 0;
        break;
    case UWBMSG_STOP:
        ret = UWBStop();
        if (!ret)
        {
            ret = _NIcreateStopped();
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        break;
    default:
        LOG_WRN("Ignoring cmd 0x%02X", inData[0]);
        break;
    }
    ret = 0;
exit:
    return ret;
}

static int _SessionStateCallback(uint32_t session_id, uint8_t state, uint8_t reason)
{
    int ret = 0;

    mNI.session_state = state;

    switch (state)
    {
    case UWB_SESSION_INITIALIZED:
        LOG_INF("Session %08X Initialized", session_id);
        break;
    case UWB_SESSION_DEINITIALIZED:
        LOG_INF("Session %08X de-Initialized", session_id);
        if (!ret)
        {
            ret = _NIcreateStopped();
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        break;
    case UWB_SESSION_ACTIVE:
        // respond with configuration data now so phone
        // wont timeout while we start our session
        ret = _NIcreateIOSacd();
        if (!ret)
        {
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        LOG_INF("Session %08X Active", session_id);
        break;
    case UWB_SESSION_IDLE:
        LOG_INF("Session %08X Idle", session_id);
        break;
    case UWB_SESSION_ERROR:
        LOG_INF("Session %08X Error %02X", session_id, reason);
        break;
    default:
        break;
    }


    return ret;
}

#ifdef CONFIG_SHELL

#include <zephyr/shell/shell.h>

static int _CmdStart( const struct shell *shell, size_t argc, char **argv )
{
    int ret = UWBStart(0x11223344);

    return ret;
}

static int _CmdStop( const struct shell *shell, size_t argc, char **argv )
{
    int ret = UWBStop();

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ni,
    SHELL_CMD(start, NULL,   " Start a ranging session\n", _CmdStart),
    SHELL_CMD(stop, NULL,    " Stop a ranging session\n", _CmdStop),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ni, &sub_ni, "Nearby Interaction", NULL);

#endif


int NIinit(void)
{
    int ret = 0;

    memset(&mNI, 0, sizeof(mNI));

    ret = UWBinit(_SessionStateCallback);
    require_noerr(ret, exit);
exit:
    return 0;
}
