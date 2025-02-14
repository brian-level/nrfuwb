#include "uwb.h"
#include "uwb_range.h"
#include "uwb_defs.h"
#include "uwb_canned.h"
#include "hbci_proto.h"
#include "uci_proto.h"
#include "uci_defs.h"
#include "uci_ext_defs.h"
#include "nrfspi.h"
#include "timesvc.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME uwb
#include "Logging.h"

// Define this non-0 to dump protocol bytes
#define DUMP_PROTO 0

#define UWB_MAX_COMMAND_SET (16)

static struct
{
    bool initialized;
    bool is_responder;
    int  power_offset;
    uint32_t session_id;
    uint8_t  channel_id;
    bool do_AoA_Calibration;
    bool do_Calibration;
    bool do_OTP_Read_Power;
    bool do_OTP_Read_XTAL;

    bool start_request;
    bool stop_request;

    enum
    {
        UWB_IDLE,
        UWB_SESSION,
        UWB_STOP,
        UWB_RX
    }
    state, next_state;

    enum
    {
        IS_INIT,
        IS_RESET,
        IS_SET_CONFIG,
        IS_READ_OTP_XTAL,
        IS_READ_OTP_TXPOWER,
        IS_CALIBRATE,
        IS_INIT_SESSION,
        IS_APP_CONFIG,
        IS_START_SESSION,
        IS_IN_SESSION,
        IS_SESSION_STOP,
        IS_WAIT_RSP,
        IS_WAIT_NTF,
    }
    init_state, next_init_state;

    int command_set_count;
    int command_set_state;
    uint32_t command_size[UWB_MAX_COMMAND_SET];
    const uint8_t *commands[UWB_MAX_COMMAND_SET];

    session_state_callback_t session_callback;
}
mUWB;

/* shared configuration data from mobile app wrapped
 * in a profile command
 */
static uint8_t UWB_PROPRIETARY_SET_PROFILE_CMD[64];
static uint32_t UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT;

/*
static int UWBRead(
                uint8_t *outData,
                int inSize,
                int *outCount)
{
    int ret = -EINVAL;

    require(outData, exit);
    require(inSize, exit);
    require(outCount, exit);

    *outCount = 0;
    ret = 0;
exit:
    return ret;
}
*/

static int UWBWrite(
                const uint8_t *inData,
                const int inCount)
{
    int ret = -EINVAL;

    require(inData, exit);
    require(inCount, exit);

    ret = UCIprotoWriteRaw(inData, inCount);
exit:
    return ret;
}

static uint8_t *_uwb_add_session_id(uint8_t *command)
{
    // todo - worry about endianess?
    memcpy(command + UWB_SESSION_ID_OFFSET_IN_CMD, &mUWB.session_id, sizeof(uint32_t));
    return command;
}

static int _uwb_initialize(
                bool    haveMessage,
                uint8_t type,
                uint8_t gid,
                uint8_t oid,
                uint8_t *payload,
                int payloadLength)
{
    int ret = 0;
    uint8_t status;

    LOG_DBG("Init UWBS state %d [%d of %d]", mUWB.init_state, mUWB.command_set_state, mUWB.command_set_count);

    if (haveMessage && (type == UCI_MT_NTF))
    {
        if (gid == UCI_GID_CORE && oid == UCI_MSG_CORE_DEVICE_STATUS_NTF)
        {
            status = 0;
            if (payloadLength > 0)
            {
                status = payload[0];
            }

            if (status)
            {
                if (mUWB.next_init_state == IS_RESET)
                {
                    LOG_INF("UWBS Ready after devid set, reset");
                    mUWB.init_state = mUWB.next_init_state;
                }
                else if (mUWB.next_init_state == IS_SET_CONFIG)
                {
                    LOG_INF("UWBS Ready after reset, set config");
                    mUWB.init_state = mUWB.next_init_state;
                }
                else
                {
                    LOG_INF("UWBS Ready, no action needed");
                }
            }
            else
            {
                LOG_INF("UWBS Not Ready");
            }
        }
        else if (gid == UCI_GID_SESSION_MANAGE && oid == UCI_MSG_SESSION_STATUS_NTF)
        {
            if (payloadLength >= 6)
            {
                uint32_t session_id;
                uint8_t  sess_state;
                uint8_t  sess_reason;

                memcpy(&session_id, payload, 4);
                sess_state  = payload[4];
                sess_reason = payload[5];

                LOG_INF("Session %08X state %02X %02X", session_id, sess_state, sess_reason);

                if (mUWB.next_init_state == IS_APP_CONFIG || mUWB.next_init_state == IS_IN_SESSION)
                {
                    if (mUWB.init_state == IS_WAIT_NTF)
                    {
                        mUWB.init_state = mUWB.next_init_state;
                    }

                    switch (sess_state)
                    {
                    case UWB_SESSION_INITIALIZED:
                        if (session_id != mUWB.session_id)
                        {
                            LOG_INF("UWBS sets session handle to %08X", session_id);
                            mUWB.session_id = session_id;
                        }
                        break;
                    case UWB_SESSION_DEINITIALIZED:
                        LOG_DBG("Session %08X de-initialized", session_id);
                        if (mUWB.session_callback)
                        {
                            mUWB.session_callback(session_id, sess_state, sess_reason);
                        }
                        break;
                    case UWB_SESSION_ACTIVE:
                        LOG_DBG("Session %08X Active!", session_id);
                        if (mUWB.session_callback)
                        {
                            mUWB.session_callback(session_id, sess_state, sess_reason);
                        }
                        break;
                    case UWB_SESSION_IDLE:
                        LOG_DBG("Session %08X idle", session_id);
                        if (mUWB.session_callback)
                        {
                            mUWB.session_callback(session_id, sess_state, sess_reason);
                        }
                        break;
                    case UWB_SESSION_ERROR:
                        LOG_DBG("Session %08X error", session_id);
                        if (mUWB.session_callback)
                        {
                            mUWB.session_callback(session_id, sess_state, sess_reason);
                        }
                        break;
                    default:
                        LOG_WRN("unhandled sess state %02X", sess_state);
                        break;
                    }
                }
                else
                {
                    LOG_WRN("new session state %02X in state %d", sess_state, mUWB.next_init_state);
                }
            }
            else
            {
                LOG_WRN("bad pl for sess ntf");
            }
        }
        else if (gid == UCI_GID_RANGE_MANAGE && oid == 0x00)
        {
            UWBrangeData(payload, payloadLength);
        }
        else if (gid == UCI_GID_PROPRIETARY_SE)
        {
            switch (oid)
            {
            case EXT_UCI_MSG_READ_CALIB_DATA_CMD:
                // use the calib data to update the commands we use to
                // setup the h/w.  this is really hacky, maybe be
                // smarter about this?
                //
                // as you can see, this is a horrific use of payload length as
                // a command descriminator.. why did nxp not just invent sub cmds?
                //
                if (payloadLength == 0x05)
                {
                    /*UWB_EXT_READ_CALIB_DATA_XTAL_CAP_NTF*/
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[8]  = payload[2];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[10] = payload[3];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5[12] = payload[4];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[8]  = payload[2];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[10] = payload[3];
                    UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9[12] = payload[4];
                    //mUWB.do_OTP_Read_XTAL = false;
                }
                else if (payloadLength == 0x06)
                {
                    /*UWB_EXT_READ_CALIB_DATA_TX_POWER_NTF*/
                    uint8_t offset;
                    offset = (uint8_t)((int)payload[2] + (int)(mUWB.power_offset + ((2.1-0.6+0.5)*4))); /* murata evk */
                    UWB_SET_CALIBRATION_TX_POWER_CH5[11] = offset;
                    UWB_SET_CALIBRATION_TX_POWER_CH9[11] = offset;
                    UWB_SET_CALIBRATION_TX_POWER_CH5[9] = payload[3];
                    UWB_SET_CALIBRATION_TX_POWER_CH9[9] = payload[3];
                    //mUWB.do_OTP_Read_Power = false;
                }
                else
                {
                    LOG_WRN("Unhandled read-calib-data ntf");
                }
                LOG_INF("Got OTP data, is=%d nis=%d", mUWB.init_state, mUWB.next_init_state);

                if (mUWB.init_state == IS_WAIT_NTF)
                {
                    mUWB.init_state = mUWB.next_init_state;
                }
                break;
            default:
                break;
            }
        }

        haveMessage = false;
    }

    if (mUWB.init_state != IS_WAIT_RSP && mUWB.command_set_count)
    {
        if (mUWB.command_set_state < mUWB.command_set_count)
        {
            // send next command to uwbs and wait for reply
            //
            ret = UWBWrite(mUWB.commands[mUWB.command_set_state], mUWB.command_size[mUWB.command_set_state]);
            mUWB.next_init_state = mUWB.init_state;
            mUWB.init_state = IS_WAIT_RSP;
        }
    }
    else
    {
        if (mUWB.init_state != IS_WAIT_RSP)
        {
            if (mUWB.stop_request)
            {
                mUWB.stop_request = false;
                mUWB.start_request = false;
                mUWB.init_state = IS_SESSION_STOP;
            };
        }
        switch (mUWB.init_state)
        {
        case IS_INIT:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_INIT_BOARD_VARIANT_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_INIT_BOARD_VARIANT;
            mUWB.command_set_state = 0;
            break;
        case IS_RESET:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_RESET_DEVICE_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_RESET_DEVICE;
            mUWB.command_set_state = 0;
            break;
        case IS_SET_CONFIG:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_CONFIG_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_CONFIG;
            mUWB.command_size[mUWB.command_set_count] = UWB_VENDOR_COMMAND_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_VENDOR_COMMAND;
            mUWB.command_size[mUWB.command_set_count] = UWB_CORE_GET_DEVICE_INFO_CMD_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_CORE_GET_DEVICE_INFO_CMD;
            mUWB.command_size[mUWB.command_set_count] = UWB_CORE_GET_CAPS_INFO_CMD_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_CORE_GET_CAPS_INFO_CMD;
            mUWB.command_size[mUWB.command_set_count] = UWB_CORE_SET_ANTENNAS_DEFINE_SIZE;
            mUWB.commands[mUWB.command_set_count++] = UWB_CORE_SET_ANTENNAS_DEFINE;
            mUWB.command_set_state = 0;
            break;
        case IS_READ_OTP_XTAL:
            mUWB.command_set_count = 0;
            if (mUWB.do_OTP_Read_XTAL)
            {
                // read calibration OTP at least once
                mUWB.command_size[mUWB.command_set_count] = UWB_EXT_READ_CALIB_DATA_XTAL_CAP_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_EXT_READ_CALIB_DATA_XTAL_CAP;
            }
            else
            {
                mUWB.init_state = IS_READ_OTP_TXPOWER;
            }
            mUWB.command_set_state = 0;
            break;
        case IS_READ_OTP_TXPOWER:
            mUWB.command_set_count = 0;
            if (mUWB.do_OTP_Read_Power)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_EXT_READ_CALIB_DATA_TX_POWER_SIZE;
                mUWB.commands[mUWB.command_set_count++] = UWB_EXT_READ_CALIB_DATA_TX_POWER;
            }
            else
            {
                mUWB.init_state = IS_APP_CONFIG;
            }
            mUWB.command_set_state = 0;
            break;
        case IS_CALIBRATE:
            mUWB.command_set_count = 0;
            if (mUWB.channel_id == 0x05)
            {
                if (mUWB.do_AoA_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH5;
                    /*
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH5;
                    */
                }
                if (mUWB.do_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH5;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_TX_POWER_CH5_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_TX_POWER_CH5;
                }
            }
            else /* channel 0x09 */
            {
                if (mUWB.do_AoA_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_OFFSET_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_THRESHOLD_PDOA_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR2_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_AOA_ANTENNAS_PDOA_CALIB_PAIR1_CH9;
                    /*
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MANUFACT_ZERO_OFFSET_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_PDOA_MULTIPOINT_CALIB_CH9;
                    */
                }
                if (mUWB.do_Calibration)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RF_CLK_ACCURACY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_RX_ANT_DELAY_CALIB_CH9;
                    mUWB.command_size[mUWB.command_set_count] = UWB_SET_CALIBRATION_TX_POWER_CH9_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = UWB_SET_CALIBRATION_TX_POWER_CH9;
                }
            }
            if (mUWB.command_set_count == 0)
            {
                mUWB.init_state = IS_START_SESSION;
            }
            mUWB.command_set_state = 0;
            break;
        case IS_INIT_SESSION:
            mUWB.command_set_count = 0;
            if (UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT == 0)
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_INIT_RANGING_SIZE;
                mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_INIT_RANGING);
            }
            else
            {
                mUWB.command_size[mUWB.command_set_count] = UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT;
                mUWB.commands[mUWB.command_set_count++] = UWB_PROPRIETARY_SET_PROFILE_CMD;
            }
            mUWB.command_set_state = 0;
            break;
        case IS_APP_CONFIG:
            mUWB.command_set_count = 0;
            if (UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT != 0)
            {
                mUWB.init_state = IS_START_SESSION;
                break;
            }

            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_APP_CONFIG_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_SET_APP_CONFIG);
            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_APP_CONFIG_NXP_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_SET_APP_CONFIG_NXP);
            if (UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT == 0)
            {
                if (mUWB.is_responder)
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_RESPONDER_CONFIG_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_SET_RESPONDER_CONFIG);
                }
                else
                {
                    mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_INITIATOR_CONFIG_SIZE;
                    mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_SET_INITIATOR_CONFIG);
                }
            }
            mUWB.command_set_state = 0;
            break;
        case IS_START_SESSION:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_SET_DEBUG_CONFIG_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_SET_DEBUG_CONFIG);
            mUWB.command_size[mUWB.command_set_count] = UWB_RANGE_START_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_RANGE_START);
            mUWB.command_set_state = 0;
            break;
        case IS_IN_SESSION:
            break;
        case IS_SESSION_STOP:
            mUWB.command_set_count = 0;
            mUWB.command_size[mUWB.command_set_count] = UWB_RANGE_STOP_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_RANGE_STOP);
            mUWB.command_size[mUWB.command_set_count] = UWB_SESSION_DEINIT_SIZE;
            mUWB.commands[mUWB.command_set_count++] = _uwb_add_session_id(UWB_SESSION_DEINIT);
            mUWB.command_set_state = 0;
            break;
        case IS_WAIT_RSP:
            if (!haveMessage)
            {
                break;
            }
            if (type != UCI_MT_RSP)
            {
                LOG_WRN("Unexpected UCI %02X %02X %02X", type, gid, oid);
                break;
            }

            status = 0;
            if (payload && payloadLength > 0)
            {
                status = payload[0];
            }

            if (status == 0)
            {
                // got an OK response, go back to state we were in
                //
                mUWB.init_state = mUWB.next_init_state;

                if (mUWB.command_set_state < mUWB.command_set_count)
                {
                    // We sent a command set ok, count that
                    //
                    mUWB.command_set_state++;
                }

                if (mUWB.command_set_state >= mUWB.command_set_count)
                {
                    mUWB.command_set_count = 0;
                    mUWB.command_set_state = 0;

                    // Finished a command set, advance state
                    //
                    switch (mUWB.init_state)
                    {
                    case IS_INIT:
                        // wait for ready ntf before doing a reset
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_RESET;
                        break;
                    case IS_RESET:
                        // wait for ready ntf before set config
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_SET_CONFIG;
                        break;
                    case IS_SET_CONFIG:
                        mUWB.init_state = IS_READ_OTP_XTAL;
                        break;
                    case IS_READ_OTP_XTAL:
                        // wait for otp notification before moving on?
                        #if  1
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_READ_OTP_TXPOWER;
                        #else
                        mUWB.init_state = IS_READ_OTP_TXPOWER;
                        #endif
                        break;
                    case IS_READ_OTP_TXPOWER:
                        // wait for otp notification before moving on?
                        #if  1
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_CALIBRATE;
                        #else
                        mUWB.init_state = IS_CALIBRATE;
                        #endif
                        break;
                    case IS_CALIBRATE:
                        mUWB.init_state = IS_INIT_SESSION;
                        break;
                    case IS_INIT_SESSION:
                        // The response to an init-ranging or set-profile comamnd
                        // contains the session handle from the device (this is
                        // an nxp extension even for init-ranging) so use the
                        // response to set our session handle
                        //
                        // it is NOT an error if there is no payload, the ntf
                        // will have the new session handle
                        //
                        if (
                                (gid == UCI_GID_SESSION_MANAGE && oid == UCI_MSG_SESSION_INIT)
                            ||  (gid == UCI_GID_PROPRIETARY_SE && oid == EXT_UCI_MSG_SET_PROFILE)
                        )
                        {
                            if (payloadLength >= 5)
                            {
                                uint32_t session_id;

                                memcpy(&session_id, payload + 1, 4);

                                LOG_INF("Response to %s sets session id to %08X",
                                        (gid == UCI_GID_SESSION_MANAGE) ? "INIT-RANGING":"SET_PROFILE", session_id);
                                mUWB.session_id = session_id;
                            }
                        }
                        // after an init-session, need to wait for an initialized
                        // notification before we can advance to config app and start
                        //
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_APP_CONFIG;
                        break;
                    case IS_APP_CONFIG:
                        mUWB.init_state = IS_START_SESSION;
                        break;
                    case IS_START_SESSION:
                        // after a start-session, need to wait for active
                        // notification to ensure we started it ok
                        //
                        mUWB.init_state = IS_WAIT_NTF;
                        mUWB.next_init_state = IS_IN_SESSION;
                        break;
                    case IS_IN_SESSION:
                        mUWB.init_state = IS_IN_SESSION;
                        break;
                    case IS_SESSION_STOP:
                        mUWB.state = UWB_IDLE;
                        NRFSPIenableChip(false);
                        break;
                    case IS_WAIT_RSP:
                    case IS_WAIT_NTF:
                        LOG_WRN("Shouldnt be here");
                        break;
                    }
                }
            }
            else
            {
                LOG_WRN("Ingore status %02X in resp", status);
            }
            break;

        case IS_WAIT_NTF:
            break;

        default:
            LOG_INF("Done with Init");
            mUWB.init_state = IS_IN_SESSION;
            mUWB.state = UWB_SESSION;
            break;
        }
    }

    return ret;
}

int UWBStart(
        const uint8_t inType,
        const uint32_t inSessionID,
        const uint8_t *inProfile,
        const int inProfileLength)
{
    int ret = -EINVAL;

    require((inSessionID != 0 || (inProfile && inProfileLength)), exit);

    if (mUWB.state != UWB_SESSION)
    {
        // invalidate any prior session
        UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT = 0;

        if (inType == UWB_DeviceType_Controller)
        {
            mUWB.is_responder = false;
        }
        else
        {
            mUWB.is_responder = true;
        }

        if (inSessionID)
        {
            mUWB.session_id = inSessionID;
            LOG_INF("Starting session %08X", inSessionID);
        }
        else
        {
            uint8_t *cmd = UWB_PROPRIETARY_SET_PROFILE_CMD;
            mUWB.session_id = 0xDEADBEEF;
            require((inProfileLength + UCI_MSG_HDR_SIZE) < sizeof(UWB_PROPRIETARY_SET_PROFILE_CMD), exit);
            cmd[0] = UCI_MTS_CMD | UCI_GID_PROPRIETARY_SE;
            cmd[1] = EXT_UCI_MSG_SET_PROFILE;
            cmd[2] = 0;
            cmd[3] = inProfileLength;
            memcpy(cmd + UCI_MSG_HDR_SIZE, inProfile, inProfileLength);
            UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT = inProfileLength + UCI_MSG_HDR_SIZE;
            LOG_INF("Starting NI Session");
        }
        mUWB.start_request = true;
        mUWB.stop_request = false;
        ret = 0;
    }
    else
    {
        LOG_WRN("Already in session, not starting");
    }

    TimeSignalApplicationEvent();
exit:
    return ret;
}

int UWBStop(void)
{
    mUWB.start_request = false;
    mUWB.stop_request = true;
    TimeSignalApplicationEvent();
    return 0;
}

bool UWBReady(void)
{
    return mUWB.state == UWB_IDLE;
}

int UWBSlice(uint32_t *delay)
{
    int ret = 0;
    bool gotMessage;
    uint8_t type;
    uint8_t gid;
    uint8_t oid;
    uint8_t *payload;
    int     payloadLength;

    if (mUWB.state != UWB_IDLE)
    {
        ret = UCIprotoSlice(&gotMessage, &type, &gid, &oid, &payload, &payloadLength, delay);
    }
    else
    {
        gotMessage = false;
        ret = 0;
    }

    switch (mUWB.state)
    {
    case UWB_IDLE:
        if (mUWB.start_request)
        {
            // Bring up the UCI interface
            // (setup SPI, load f/w and init UCI)
            //
            ret = UCIprotoInit();

            mUWB.start_request = false;
            mUWB.state = UWB_SESSION;
            mUWB.init_state = IS_INIT;
            mUWB.command_set_count = 0;
            mUWB.command_set_state = 0;
            *delay = 20; // let chip boot
        }
        break;
    case UWB_SESSION:
        if (UCIready())
        {
            ret = _uwb_initialize(gotMessage, type, gid, oid, payload, payloadLength);
            if (mUWB.init_state == IS_WAIT_RSP || mUWB.init_state == IS_WAIT_NTF)
            {
                // SPI interrupt will shorten delat in wait-app-event in main loop
                *delay = 100;
            }
            else if (mUWB.init_state == IS_IN_SESSION)
            {
                *delay = 20;
            }
            else
            {
                // go right to next command send, no delay
                *delay = 0;
            }
        }
        break;
    case UWB_STOP:
        if (UCIready())
        {
            ret = UWBWrite(_uwb_add_session_id(UWB_SESSION_DEINIT), UWB_SESSION_DEINIT_SIZE);
            mUWB.state = UWB_RX;
            mUWB.next_state = UWB_IDLE;
        }
        break;
    case UWB_RX:
        if (gotMessage)
        {
            *delay = 0;
            mUWB.state = mUWB.next_state;
        }
        break;
    }
//exit:
    return ret;
}

int UWBinit(session_state_callback_t inSessionStateCallback)
{
    int ret = 0;

    memset(&mUWB, 0, sizeof(mUWB));

    mUWB.session_callback = inSessionStateCallback;

    mUWB.power_offset = 0;
    mUWB.do_OTP_Read_XTAL = true;
    mUWB.do_OTP_Read_Power = true;

    mUWB.init_state = 0;
    mUWB.command_set_state = 0;
    mUWB.command_set_count = 0;

    /* note this has to exactly match "other radio" of
     * the ranging session to work, so beware
     */
    mUWB.session_id = 0x11223344;
    mUWB.channel_id = 0x09;
    mUWB.is_responder = true;

    mUWB.do_AoA_Calibration = true;
    mUWB.do_Calibration = true;

    mUWB.start_request = false;
    mUWB.stop_request = false;

    mUWB.state = UWB_IDLE;
    mUWB.initialized = true;

    UWB_PROPRIETARY_SET_PROFILE_CMD_COUNT = 0;

    //mUWB.start_request = true;
    ret = 0;

    return ret;
}

