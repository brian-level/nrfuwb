
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

#define NI_MAX_MESSAGE  256

static struct
{
    bool        were_initiator;
    void        *conn_ctx;
    uint8_t     msgbuf[NI_MAX_MESSAGE];
    uint32_t    msgcnt;

    uint8_t     session_state;

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

#if 0
/* ------- from us to phone: accesory config data
*/
    uint8_t customerSpecMajorVer[MAX_SPEC_VER_LEN];
    uint8_t customerSpecMinorVer[MAX_SPEC_VER_LEN];
    uint8_t preferedUpdateRate;
    uint8_t RFU[10];
    /* uwb config data follows */
    ...

/* uwb config data
*/
    uint8_t length;
    uint8_t uwb_spec_ver_major[MAX_SPEC_VER_LEN];
    uint8_t uwb_spec_ver_minor[MAX_SPEC_VER_LEN];
    uint8_t manufacturer_id[4];
    uint8_t model_id[4];
    uint8_t mw_version[4];
    uint8_t ranging_role;
    uint8_t device_mac_addr[MAC_SHORT_ADD_LEN];
    uint8_t clock_drift[2]; /* minor ver 1 only? */

    /* Example of config:
      0x01, 0x00,
      0x01, 0x00,
      0x32, 0x11, 0x10, 0x00,
      0x46, 0x01, 0x31, 0x00,
      0x00, 0x00, 0x06, 0x04,
      0x01,
      0x05, 0xda,
      0x64, 0x00
    */
/* ------- from phone to us:
*/
    /* Example of shared config data (30 opaque bytes) */
     0x01, 0x00,    /* v maj */
     0x01, 0x00,    /* v min */
     0x19,          /* profile blob length */
     0x55, 0x53, 0x08, 0x51,  /* 25 (v1.1) or 23 (v1.0) blob bytes */
     0x00, 0x00, 0x0b, 0x09,
     0x06, 0x00, 0x10, 0x0e,
     0xc6, 0x00, 0x03, 0x17,
     0x51, 0xef, 0x27, 0xfb,
     0xdd, 0x70, 0x75, 0x64,
     0x00

#endif

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

static int _NIcreateIOSacd(void)
{
    // Nearby-Interaction-Accessory-Protocol-Specification-Release-R2-1.pdf

    uint8_t *cursor = mNI.msgbuf;
    int room = sizeof(mNI.msgbuf);
    int pad;
    int ret;

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
    uint8_t dstMac[2];

    require(inData, exit);
    require(inCount > 0, exit);

    mNI.conn_ctx = conn_ctx;

    LOG_INF("Got Message %02X %u bytes", inData[0], inCount);

    sessionID = 0x11223344;

    switch (inData[0])
    {
    case UWBMSG_INITIALIZE_IOS:
        ret = _NIcreateIOSacd();
        if (!ret)
        {
            ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
        }
        break;
    case UWBMSG_INITIALIZE_ANDROID:
        LOG_WRN("Android not supported yet");
        break;
    case UWBMSG_CONFIG_AND_START:
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
            ret = UWBStart(mNI.were_initiator  ? UWB_DeviceType_Controller : UWB_DeviceType_Controlee,
                        0, mNI.msgbuf, mNI.msgcnt);
        }
        break;
    case UWBMSG_STOP:
        ret = UWBStop();
        //ret = _NIcreateStopped();
        //ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
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
        LOG_INF("Session %08X Active", session_id);
        ret = _NIcreateStarted();
        ret = _NItxMessage(mNI.msgbuf, mNI.msgcnt);
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
    int ret = UWBStart(UWB_DeviceType_Controlee, 0x11223344, NULL, 0);

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

    ret = UWBinit(_SessionStateCallback);
    require_noerr(ret, exit);
exit:
    return 0;
}
