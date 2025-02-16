
#include "nearby_interaction.h"
#include "uwb.h"
#include "uwb_defs.h"
#include "uci_defs.h"
#include "level_ble.h"
#include "ble_internal.h"
#include "timesvc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME ni
#include "Logging.h"

#define NI_MAX_MESSAGE  256

static struct
{
    bool        were_initiator;
    uint8_t     client_type;
    uint64_t    start_timer;
    const void *ble_conn_ctx;
    uint8_t     msgbuf[NI_MAX_MESSAGE];
    uint32_t    msgcnt;

    enum {
        SS_INACTIVE,    // nothing happening
        SS_STARTING,    // we asked session to start
        SS_INIT,        // uwb told us it inited a session
        SS_IDLE,        // uwb told us session in created and ready
        SS_ACTIVE,      // uwb told us session is running
        SS_OVER         // for any reason, session is stopping
    }
    session_state;

    uint8_t     our_device_type;
    uint8_t     our_device_role;
    uint8_t     our_mac_addr[2];
    uint16_t    our_uwb_ver[2];
    uint16_t    our_mw_ver[2];
    uint8_t     our_clock_drift[2];
    uint8_t     our_manuf_id[4];
    uint8_t     our_model_id[4];

    uint8_t     shared_data_blob[UWB_PROFILE_BLOB_SIZE_v1_1];
    uint8_t     shared_data_blob_length;
}
mNI;

#define NI_VER_MAJ      (1)
#define NI_VER_MIN      (0)

#define ACD_VER_MAJ     (1)
#define ACD_VER_MIN     (1)

#define ACD_DEV_ROLE    (1)

#define SHORT_MAC_ADDRESS_MODE (0x00)
#define EXTENDED_MAC_ADDRESS_MODE_WITH_HEADER (0x02)
#define MAC_SHORT_ADD_LEN   (2)
#define MAX_SPEC_VER_LEN    (2)

#define UPDATE_RATE_AUTO    (0)
#define UPDATE_RATE_MIN     (10)
#define UPDATE_RATE_MAX     (20)

#define ACD_CONFIG_LEN      (21)

#define NI_CLIENT_TYPE_IOS      (0)
#define NI_CLIENT_TYPE_ANDROID  (1)

static int _NIputUINT16(uint8_t **pcursor, int *room, const uint16_t data)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;

    require(pcursor && *pcursor && room, exit);
    require(*room >= sizeof(uint16_t), exit);
    *cursor++ = data & 0xFF;
    *cursor++ = data >> 8;
    *pcursor = cursor;
    *room = *room - sizeof(uint16_t);
    ret = 0;
exit:
    return ret;
}

static int _NIputUINT8(uint8_t **pcursor, int *room, const uint8_t data)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;

    require(pcursor && *pcursor && room, exit);
    require(&room > 0, exit);
    *cursor++ = data;
    *pcursor = cursor;
    *room = *room - 1;
    ret = 0;
exit:
    return ret;
}

static int _NIputDATA(uint8_t **pcursor, int *room, const uint8_t *data, const int len)
{
    uint8_t *cursor = *pcursor;
    int ret = -EINVAL;
    int i;

    require(pcursor && *pcursor && room, exit);
    require(*room >= len, exit);

    for (i = 0; i < len; i++)
    {
        *cursor++ = data[i];
    }

    *pcursor = cursor;
    *room = *room - len;
    ret = 0;
exit:
    return ret;
}

static int _NIcreateProfile(void)
{
    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    int needed = 5 + mNI.shared_data_blob_length;
    int ret = -EINVAL;

    require(room >= needed, exit);

    // create the UCI PROP_SET_PROFILE command payload
    //
    ret  = _NIputUINT8(&cursor, &room, UWB_Profile_1);
    ret |= _NIputUINT8(&cursor, &room, mNI.our_device_type);
    ret |= _NIputDATA(&cursor, &room, mNI.our_mac_addr, sizeof(mNI.our_mac_addr));
    ret |= _NIputUINT8(&cursor, &room, mNI.our_device_role);
    ret |= _NIputDATA(&cursor, &room, mNI.shared_data_blob, mNI.shared_data_blob_length);
    require_noerr(ret, exit);

    mNI.msgcnt = cursor - mNI.msgbuf;
    require(mNI.msgcnt == needed, exit);

    LOG_HEXDUMP_INF(mNI.msgbuf, mNI.msgcnt, "Profile Info cmd");
exit:
    return ret;

}

static int _NIcreateIOSacd(uint8_t client_type)
{
    // Nearby-Interaction-Accessory-Protocol-Specification-Release-R2-1.pdf

    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    int pad;
    int ret;

#if 1 // be the controller
    mNI.were_initiator = true;
    mNI.our_device_type = UWB_DeviceType_Controller;
    mNI.our_device_role = UWB_DeviceRole_Initiator;
    mNI.our_mac_addr[0] = 0x11;
    mNI.our_mac_addr[1] = 0x11;
#else
    mNI.were_initiator = false;
    mNI.our_device_type = UWB_DeviceType_Controlee;
    mNI.our_device_role = UWB_DeviceRole_Responder;
    mNI.our_mac_addr[0] = 0x22;
    mNI.our_mac_addr[1] = 0x22;
#endif
    if (client_type == NI_CLIENT_TYPE_IOS)
    {
        ret = _NIputUINT8(&cursor, &room, UWBMSG_CONFIG_DATA);   // header cmd for dispatch
        require_noerr(ret, exit);

        ret = _NIputUINT16(&cursor, &room, NI_VER_MAJ);      // MajorVersion
        require_noerr(ret, exit);
        ret = _NIputUINT16(&cursor, &room, NI_VER_MIN);      // MinorVersion
        require_noerr(ret, exit);
        ret = _NIputUINT8(&cursor, &room, UPDATE_RATE_MIN);  // PreferredUpdateRate
        require_noerr(ret, exit);

        for (pad = 0; pad < 10; pad++)
        {
            ret = _NIputUINT8(&cursor, &room, 0);            // Reserved
            require_noerr(ret, exit);
        }

        ret = _NIputUINT8(&cursor, &room, ACD_CONFIG_LEN);   // UWBconfigDataLength
        require_noerr(ret, exit);

        ret = _NIputUINT16(&cursor, &room, mNI.our_uwb_ver[0]);     // MajorVersion
        require_noerr(ret, exit);
        ret = _NIputUINT16(&cursor, &room, mNI.our_uwb_ver[1]);     // MinorVersion
        require_noerr(ret, exit);

        ret = _NIputDATA(&cursor, &room, mNI.our_manuf_id, sizeof(mNI.our_manuf_id));
        require_noerr(ret, exit);
        ret = _NIputDATA(&cursor, &room, mNI.our_model_id, sizeof(mNI.our_model_id));
        require_noerr(ret, exit);

        ret = _NIputUINT16(&cursor, &room, mNI.our_mw_ver[0]);
        require_noerr(ret, exit);
        ret = _NIputUINT16(&cursor, &room, mNI.our_mw_ver[1]);
        require_noerr(ret, exit);

        ret = _NIputUINT8(&cursor, &room, mNI.our_device_role);     // ranging role
        require_noerr(ret, exit);

        ret = _NIputDATA(&cursor, &room, mNI.our_mac_addr, sizeof(mNI.our_mac_addr));
        require_noerr(ret, exit);
        ret = _NIputDATA(&cursor, &room, mNI.our_clock_drift, sizeof(mNI.our_clock_drift));
        require_noerr(ret, exit);
    }
    else if (client_type == NI_CLIENT_TYPE_ANDROID)
    {
    }
    else
    {
        LOG_ERR("No such client type %u", client_type);
    }
    mNI.msgcnt = cursor - mNI.msgbuf;

    LOG_HEXDUMP_INF(mNI.msgbuf, mNI.msgcnt, "ACD");
exit:
    return ret;
}

static int _NIcreateStarted(void)
{
    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    int ret;

    ret = _NIputUINT8(&cursor, &room, UWBMSG_DID_START);   // heasder cmd for dispatch
    mNI.msgcnt = cursor - mNI.msgbuf;
    return ret;
}

static int _NIcreateStopped(void)
{
    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    int ret;

    ret = _NIputUINT8(&cursor, &room, UWBMSG_DID_START);   // heasder cmd for dispatch
    mNI.msgcnt = cursor - mNI.msgbuf;
    return ret;
}

static int _NIgetUINT16(uint8_t **pcursor, int *have, uint16_t *value)
{
    int ret = -EINVAL;
    uint16_t val;
    uint8_t *cursor = *pcursor;

    require(*have >= sizeof(uint16_t), exit);

    val = (uint16_t)*cursor++;
    val |= ((uint16_t)*cursor++) << 8;
    *value = val;
    *pcursor = cursor;
    *have = *have - sizeof(uint16_t);
    ret = 0;
exit:
    return ret;
}

static int _NIgetUINT8(uint8_t **pcursor, int *have, uint8_t *value)
{
    int ret = -EINVAL;
    uint8_t val;
    uint8_t *cursor = *pcursor;

    require(*have >= sizeof(uint8_t), exit);

    val = (uint16_t)*cursor++;
    *value = val;
    *pcursor = cursor;
    *have = *have - sizeof(uint8_t);
    ret = 0;
exit:
    return ret;
}

static int _NIparseIOSscd(
                const uint8_t *inData,
                const int inCount,
                uint32_t *outSessionID,
                uint8_t *outDstMac)
{
    int ret = -EINVAL;
    uint8_t *cursor = (uint8_t*)inData;
    int remain;
    uint8_t payloadLength;
    uint16_t vers_maj;
    uint16_t vers_min;

    LOG_HEXDUMP_INF(inData, inCount, "Shared CD");

    remain = inCount;

    require(remain >= 28, exit);

    /* version maj/min */
    ret = _NIgetUINT16(&cursor, &remain, &vers_maj);
    require_noerr(ret, exit);
    ret = _NIgetUINT16(&cursor, &remain, &vers_min);
    require_noerr(ret, exit);

    require(vers_maj =- 0x0001, exit);
    require((vers_min == 0x0001 || vers_min == 0x0000), exit);

    ret = _NIgetUINT8(&cursor, &remain, &payloadLength);
    require_noerr(ret, exit);
    require(payloadLength <= remain, exit);

    if (vers_min == 1)
    {
        require(inCount == UWB_PROFILE_BLOB_SIZE_v1_1, exit);
    }
    else if (vers_min == 0)
    {
        require(inCount == UWB_PROFILE_BLOB_SIZE_v1_0, exit);
    }

    memcpy(mNI.shared_data_blob, inData, inCount);
    mNI.shared_data_blob_length = inCount;

exit:
    return ret;
}

static int _NItxMessage(const uint8_t *inData, const uint32_t inCount)
{
    int ret;

    if (mNI.ble_conn_ctx)
    {
        // TODO - what if conn_ctx is an int handle and 0 is valid?
        // maybe use a .is_conn_ctx_set member?
        //
        ret = BLEinternalNotifyUWB((void*)mNI.ble_conn_ctx, (void*)inData, inCount);
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
    uint8_t dstMac[2];

    require(inData, exit);
    require(inCount > 0, exit);

    if (conn_ctx != mNI.ble_conn_ctx)
    {
        LOG_ERR("Rx BLE on different connection?");
        mNI.ble_conn_ctx = conn_ctx;
    }

    LOG_INF("Got Message %02X %u bytes", inData[0], inCount);

    sessionID = 0x11223344;

    switch (inData[0])
    {
    case UWBMSG_INITIALIZE_IOS:
        // iOS client wants to start a session.  We respond
        // with an AccessoryConfigurationData payload
        //
        if (mNI.session_state == SS_INACTIVE)
        {
            mNI.client_type = NI_CLIENT_TYPE_IOS;
            ret = _NIcreateIOSacd(NI_CLIENT_TYPE_IOS);
            if (!ret)
            {
                ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
                mNI.session_state = SS_STARTING;
                mNI.start_timer = TimeUptimeMilliseconds();
            }
        }
        else
        {
            LOG_WRN("Ignoring Init iOS because already active");
        }
        break;
    case UWBMSG_INITIALIZE_ANDROID:
        if (mNI.session_state == SS_INACTIVE)
        {
            mNI.client_type = NI_CLIENT_TYPE_ANDROID;
            ret = _NIcreateIOSacd(NI_CLIENT_TYPE_ANDROID);
            if (!ret)
            {
                ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
                mNI.session_state = SS_STARTING;
                mNI.start_timer = TimeUptimeMilliseconds();
            }
        }
        else
        {
            LOG_WRN("Ignoring Init Android because already active");
        }
        break;
    case UWBMSG_CONFIG_AND_START:
        // Mobile client is giving us a ShareableConfigurationData blob
        // which we will pass to the UWB layer and start a session
        //
        ret = _NIparseIOSscd(inData + 1, inCount - 1, &sessionID, dstMac);
        if (ret)
        {
            LOG_ERR("Cant parse shared data");
            break;
        }

        ret = _NIcreateProfile();
        if (ret)
        {
            LOG_ERR("Can't create profile data");
            break;
        }

        if (!ret)
        {
            ret = UWBstart(mNI.were_initiator  ? UWB_DeviceType_Controller : UWB_DeviceType_Controlee,
                        0, mNI.msgbuf, mNI.msgcnt);
            if (ret)
            {
                LOG_WRN("Can't start session");
                if (mNI.ble_conn_ctx)
                {
                    ret = _NIcreateStopped();
                    ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
                }
            }
        }
        break;
    case UWBMSG_STOP:
        ret = UWBstop();
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
        mNI.session_state = SS_INIT;
        break;
    case UWB_SESSION_DEINITIALIZED:
        LOG_INF("Session %08X de-Initialized", session_id);
        mNI.session_state = SS_OVER;
        break;
    case UWB_SESSION_ACTIVE:
        LOG_INF("Session %08X Active", session_id);
        // Tell mobile client we've started if connected
        //
        if (mNI.ble_conn_ctx)
        {
            ret = _NIcreateStarted();
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        else
        {
            ret = 0;
        }
        if (!ret)
        {
            mNI.session_state = SS_ACTIVE;
        }
        break;
    case UWB_SESSION_IDLE:
        LOG_INF("Session %08X Idle", session_id);
        mNI.session_state = SS_IDLE;
        break;
    case UWB_SESSION_ERROR:
        LOG_INF("Session %08X Error %02X", session_id, reason);
        mNI.session_state = SS_OVER;
        break;
    default:
        break;
    }

    if (mNI.session_state == SS_OVER)
    {
        LOG_INF("Session %0X complete", session_id);
        if (mNI.session_state != SS_INACTIVE && mNI.session_state != SS_OVER)
        {
            // Inform mobile session is over
            //
            if (mNI.ble_conn_ctx)
            {
                ret = _NIcreateStopped();
                ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
            }
        }

        mNI.session_state = SS_INACTIVE;

        // make sure underlying session is stopped for sure
        //
        UWBstop();
    }

    return ret;
}

int NIbleConnectHandler(const void * const inConnectionHandle, const uint16_t inMTU, const bool isConnected)
{
    int ret = 0;
    uint32_t session_id;
    uint8_t sess_state;

    if (isConnected)
    {
        mNI.ble_conn_ctx = inConnectionHandle;
    }
    else
    {
        mNI.ble_conn_ctx = NULL;

        if (mNI.session_state != SS_INACTIVE)
        {
            LOG_INF("BLE disconnect stops ranging session");
            UWBstop();
        }
    }

    // when connected, we have no idea what state UWB is in so ask
    //
    ret = UWBgetSessionState(&session_id, &sess_state);
    if (!ret)
    {
        switch (sess_state)
        {
        case UWB_SESSION_INITIALIZED:
            mNI.session_state = SS_INIT;
            break;
        case UWB_SESSION_DEINITIALIZED:
            mNI.session_state = SS_INACTIVE;
            break;
        case UWB_SESSION_ACTIVE:
            mNI.session_state = SS_ACTIVE;
            break;
        case UWB_SESSION_IDLE:
            mNI.session_state = SS_IDLE;
            break;
        default:
        case UWB_SESSION_ERROR:
            mNI.session_state = SS_INACTIVE;
            break;
        }
    }
    // note, returning non-0 here will close the ble connection
    return ret;
}

int NIslice(uint32_t *delay)
{
    return UWBslice(delay);
}

#ifdef CONFIG_SHELL

#include <zephyr/shell/shell.h>

static int _CmdStart( const struct shell *shell, size_t argc, char **argv )
{
    bool initiate = false;
    uint32_t session_id = 0x11223344;

    if (argc > 1)
    {
        switch ((*++argv)[0])
        {
        case 'i':
        case 'I':
            initiate = true;
            break;
        }

        if (argc > 2)
        {
            session_id = strtoul(*++argv, NULL, 0);
        }
    }

    if (initiate)
    {
        mNI.were_initiator = true;
        mNI.our_device_type = UWB_DeviceType_Controller;
        mNI.our_device_role = UWB_DeviceRole_Initiator;
        mNI.our_mac_addr[0] = 0x11;
        mNI.our_mac_addr[1] = 0x11;
    }
    else
    {
        mNI.were_initiator = false;
        mNI.our_device_type = UWB_DeviceType_Controlee;
        mNI.our_device_role = UWB_DeviceRole_Responder;
        mNI.our_mac_addr[0] = 0x22;
        mNI.our_mac_addr[1] = 0x22;
    }

    shell_print(shell, "Starting %s ranging session id %08X",
            initiate ? "initator" : "responder", session_id);

    int ret = UWBstart(mNI.our_device_type, session_id, NULL, 0);

    return ret;
}

static int _CmdStop( const struct shell *shell, size_t argc, char **argv )
{
    int ret = UWBstop();

    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ni,
    SHELL_CMD_ARG(start, NULL,
            " Start ranging session [i|r] [sessoion id]\n",
            _CmdStart, 0, 2),
    SHELL_CMD(stop, NULL,
            " Stop ranging session\n",
            _CmdStop),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ni, &sub_ni, "Nearby Interaction", NULL);

#endif


int NIinit(void)
{
    int ret = 0;

    memset(&mNI, 0, sizeof(mNI));

    mNI.our_uwb_ver[0] = 1;
    mNI.our_uwb_ver[1] = 1;

    mNI.our_mw_ver[0] = 1;
    mNI.our_mw_ver[1] = 1;

    mNI.our_manuf_id[0] = 0x32;
    mNI.our_manuf_id[1] = 0x11;
    mNI.our_manuf_id[2] = 0x10;
    mNI.our_manuf_id[3] = 0x00;

    mNI.our_model_id[0] = 0x46;
    mNI.our_model_id[1] = 0x01;
    mNI.our_model_id[2] = 0x31;
    mNI.our_model_id[3] = 0x00;

    mNI.our_clock_drift[0] = 0x64;
    mNI.our_clock_drift[1] = 0x00;

    mNI.session_state = SS_INACTIVE;
    ret = UWBinit(_SessionStateCallback);
    require_noerr(ret, exit);
exit:
    return 0;
}

