#include "uci_proto.h"
#include "uci_defs.h"
#include "uci_ext_defs.h"
#include "nrfspi.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME uciproto
#include "Logging.h"

// Define this non-0 to dump protocol bytes
#define DUMP_PROTO 1

#define UCI_RESP_TIMEOUT    (30)

static struct
{
    enum {
        UCI_BOOT,
        UCI_INIT,
        UCI_OFFLINE,
        UCI_RESET,
        UCI_IDLE,
        UCI_TX,
        UCI_RX,
    }
    state, nextstate;

    enum {
        UWB_INACTIVE,
        UWB_ACTIVE,
    }
    uwb_state;

    bool    needs_reset;
    bool    got_dev_info;
    bool    got_cap_info;

    uint64_t cmd_start;
    uint32_t timeout_count;

    int     max_packet;
    uint8_t txbuf[UCI_MSG_HDR_SIZE + UCI_MAX_PAYLOAD_SIZE];
    int     txcnt;
    uint8_t rxbuf[UCI_MAX_PAYLOAD_SIZE];
    int     rxcnt;
}
mUCI;

static void uci_dump(
                const char *inBlurb,
                uint8_t inType,
                uint8_t inGID,
                uint8_t inOID,
                const uint8_t *inData,
                const int inCount)
{
    const char *statestr;
    const char *typestr;
    const char *gidstr;
    const char *oidstr = "????";
    const char *devstatstr = "";

    switch (mUCI.state)
    {
    case UCI_BOOT:      statestr = "BOOT"; break;
    case UCI_INIT:      statestr = "INIT"; break;
    case UCI_OFFLINE:   statestr = "OFFLINE"; break;
    case UCI_RESET:     statestr = "RESET"; break;
    case UCI_IDLE:      statestr = "IDLE"; break;
    case UCI_TX:        statestr = "TX"; break;
    case UCI_RX:        statestr = "RX"; break;
    default:            statestr = "????"; break;
    }

    switch (inType)
    {
    case UCI_MT_DATA:   typestr = "dat"; break; /* proprietary user of 0 by NxP */
    case UCI_MT_CMD:    typestr = "cmd"; break;
    case UCI_MT_RSP:    typestr = "rsp"; break;
    case UCI_MT_NTF:    typestr = "ntf"; break;
    default:            typestr = "???"; break;
    }

    switch (inGID)
    {
    case UCI_GID_CORE:
        gidstr =  "core";
        switch (inOID)
        {
        case UCI_MSG_CORE_DEVICE_RESET:         oidstr = "reset"; break;
        case UCI_MSG_CORE_DEVICE_STATUS_NTF:
            if (inCount > 0)
            {
                switch (inData[0])
                {
                case 0:     devstatstr = "init (properietary)"; break;
                case 1:     devstatstr = "ready"; break;
                case 2:     devstatstr = "active"; break;
                case 0xFE:  devstatstr = "h/w hang"; break;
                case 0xFF:  devstatstr = "error"; break;
                default:    devstatstr = "rsrvd"; break;
                }
            }
            oidstr = "status ntf";
            break;
        case UCI_MSG_CORE_DEVICE_INFO:          oidstr = "info"; break;
        case UCI_MSG_CORE_GET_CAPS_INFO:        oidstr = "caps"; break;
        case UCI_MSG_CORE_SET_CONFIG:           oidstr = "set cfg"; break;
        case UCI_MSG_CORE_GET_CONFIG:           oidstr = "get cfg"; break;
        case UCI_MSG_CORE_DEVICE_SUSPEND:       oidstr = "suspend"; break;
        case UCI_MSG_CORE_GENERIC_ERROR_NTF:    oidstr = "err nrf"; break;
        case UCI_MSG_CORE_QUERY_UWBS_TIMESTAMP: oidstr = "time"; break;
        default: break;
        }
        break;
    case UCI_GID_SESSION_MANAGE:    gidstr =  "session cfg"; break;
    case UCI_GID_RANGE_MANAGE:      gidstr =  "range mngmt"; break;
    case UCI_GID_DATA_CONTROL:      gidstr =  "data ctrl"; break;
    case UCI_GID_TEST:              gidstr =  "test"; break;
    case UCI_GID_PROPRIETARY:       gidstr =  "proprietary"; break;
    case UCI_GID_VENDOR:            gidstr =  "vendor"; break;
    case UCI_GID_PROPRIETARY_SE:    gidstr =  "prop se"; break;
    case UCI_GID_INTERNAL_GROUP:    gidstr =  "internal"; break;
    case UCI_GID_INTERNAL:          gidstr =  "int mw dm"; break;
    default:                        gidstr = "????"; break;
    }

    LOG_INF("UCI %s %s %02X %02X %02X with %d bytes:  %s %s %s %s",
                statestr,
                inBlurb, inType, inGID, inOID, inCount,
                typestr, gidstr, oidstr, devstatstr);
    if (inCount)
    {
        LOG_HEXDUMP_INF(inData, inCount, "Data:");
    }
}

static void uci_decode(
                uint8_t inType,
                uint8_t inGID,
                uint8_t inOID,
                uint8_t *inData,
                const int inCount)
{
    switch (inType)
    {
    case UCI_MT_DATA:
    case UCI_MT_NTF:
        switch (inGID)
        {
        case UCI_GID_CORE:
            switch (inOID)
            {
            case UCI_MSG_CORE_DEVICE_RESET:
                break;
            case UCI_MSG_CORE_DEVICE_STATUS_NTF:
                if (inCount > 0)
                {
                    if (inData[0] == 0)
                    {
                        // NXP proprietary use of device status 0 as
                        // "init, ready to get a proprietary init sequence"
                        //
                        if (mUCI.state == UCI_BOOT)
                        {
                            LOG_INF("UWB Device Status Init, Booting");
                            mUCI.state = UCI_INIT;
                        }
                    }
                    else if (inData[0] == 1)
                    {
                        LOG_INF("UWB Device Ready");

                        if (mUCI.state == UCI_OFFLINE)
                        {
                            LOG_INF("Ready, go to IDLE state");
                            mUCI.state = UCI_IDLE;
                        }
                    }
                    else if (inData[0] == 2)
                    {
                        LOG_INF("UWB Device Active");
                        mUCI.uwb_state = UWB_ACTIVE;
                    }
                    else if (inData[0] == 0xFE || inData[0] == 0xFF)
                    {
                        LOG_INF("UWB Device Error/Hang, resetting");
                        // TODO - toggle power?
                        NRFSPIinit();
                        mUCI.state = UCI_BOOT;
                    }
                }
                break;
            case UCI_MSG_CORE_DEVICE_INFO:
            case UCI_MSG_CORE_GET_CAPS_INFO:
            case UCI_MSG_CORE_SET_CONFIG:
            case UCI_MSG_CORE_GET_CONFIG:
            case UCI_MSG_CORE_DEVICE_SUSPEND:
                break;
            case UCI_MSG_CORE_GENERIC_ERROR_NTF:
                if (inCount > 0 && inData[0] == 0xA)
                {
                    LOG_WRN("Resend request in state %d", mUCI.state);
                    // repeat last command
                    mUCI.state = UCI_TX;
                }
                break;
            case UCI_MSG_CORE_QUERY_UWBS_TIMESTAMP:
            default:
                break;
            }
            break;
        case UCI_GID_SESSION_MANAGE:
        case UCI_GID_RANGE_MANAGE:
        case UCI_GID_DATA_CONTROL:
        case UCI_GID_TEST:
        case UCI_GID_PROPRIETARY:
        case UCI_GID_VENDOR:
        case UCI_GID_PROPRIETARY_SE:
        case UCI_GID_INTERNAL_GROUP:
        case UCI_GID_INTERNAL:
        default:
            break;
        }
        break;
    case UCI_MT_CMD:
        LOG_ERR("Command from dev?");
        break;
    case UCI_MT_RSP:
        mUCI.state = mUCI.nextstate;
        switch (inGID)
        {
        case UCI_GID_CORE:
            switch (inOID)
            {
            case UCI_MSG_CORE_DEVICE_RESET:
                LOG_INF("Reset complete");
                mUCI.needs_reset = false;
                mUCI.txcnt = 0;
                break;
            case UCI_MSG_CORE_DEVICE_STATUS_NTF:
                break;
            case UCI_MSG_CORE_DEVICE_INFO:
                mUCI.got_dev_info = true;
                mUCI.txcnt = 0;
                break;
            case UCI_MSG_CORE_GET_CAPS_INFO:
                mUCI.got_cap_info = true;
                mUCI.txcnt = 0;
                break;
            case UCI_MSG_CORE_SET_CONFIG:
            case UCI_MSG_CORE_GET_CONFIG:
            case UCI_MSG_CORE_DEVICE_SUSPEND:
                mUCI.txcnt = 0;
                break;
            case UCI_MSG_CORE_GENERIC_ERROR_NTF:
                LOG_WRN("Error response, resetting");
                mUCI.state = UCI_RESET;
                mUCI.txcnt = 0;
                break;
            case UCI_MSG_CORE_QUERY_UWBS_TIMESTAMP:
            default:
                break;
            }
            break;
        case UCI_GID_SESSION_MANAGE:
        case UCI_GID_RANGE_MANAGE:
        case UCI_GID_DATA_CONTROL:
        case UCI_GID_TEST:
            break;
        case UCI_GID_PROPRIETARY:
            if (inOID == EXT_UCI_MSG_CORE_DEVICE_INIT)
            {
                // getting a response to our init command means
                // we can do a normal startup when we get an ok status ntf
                //
                if (inCount > 0 && inData[0] == 0)
                {
                    LOG_INF("UWB pre-Init OK");
                    mUCI.state = UCI_OFFLINE;
                }
            }
            mUCI.txcnt = 0;
            break;
        case UCI_GID_VENDOR:
        case UCI_GID_PROPRIETARY_SE:
        case UCI_GID_INTERNAL_GROUP:
        case UCI_GID_INTERNAL:
        default:
            break;
        }
        break;
    default:
        break;
    }
}

static int _UCItxCommand(void)
{
    int ret = -EINVAL;
    int chunk;
    int total;
    int remain;
    uint8_t *header;
    uint8_t *payload;

    header = mUCI.txbuf;
    payload = mUCI.txbuf + UCI_MSG_HDR_SIZE;
    total = 0;
    remain = mUCI.txcnt - UCI_MSG_HDR_SIZE;

    require(remain >= 0, exit);

    do
    {
        chunk = remain - total;

        header[0] &= ~UCI_PBF_MASK;

        if (chunk > mUCI.max_packet)
        {
            chunk = mUCI.max_packet;
            header[0] |= UCI_PBF_MASK;
        }

        header[3] = chunk;

        // xfer header
        ret = NRFSPIwrite(header, 4);
        require_noerr(ret, exit);

        if (chunk)
        {
            k_sleep(K_USEC(80));

            // xfer chunk
            ret = NRFSPIwrite(payload, chunk);

            total += chunk;
            payload += chunk;
        }
    }
    while (!ret && (total < remain));
exit:
    return ret;
}

int UCIprotoRead(
                uint8_t *outType,
                uint8_t *outGID,
                uint8_t *outOID,
                uint8_t *outData,
                int inSize,
                int *outCount)
{
    int ret = -1;
    uint8_t header[4];
    int payload_length;
    uint8_t frag;

    require(outData, exit);
    require(inSize > 0, exit);

    // read header
    ret = NRFSPIread(header, sizeof(header), true);
    require_noerr(ret, exit);

    if (outType)
    {
        *outType = (header[0] & UCI_MT_MASK) >> UCI_MT_SHIFT;
    }

    if (outGID)
    {
        *outGID = (header[0] & UCI_GID_MASK) >> UCI_GID_SHIFT;
    }

    if (outOID)
    {
        *outOID = (header[1] & UCI_OID_MASK) >> UCI_OID_SHIFT;
    }

    if (outCount)
    {
        *outCount = 0;
    }

    frag = header[0] & UCI_PBF_MASK;
    payload_length = header[3];

    if (header[1] & 0x80)
    {
        // extended payload - length
        LOG_ERR("Ext payload");
    }

    require(payload_length <= inSize, exit);

    ret = NRFSPIread(outData, payload_length, true);
    require_noerr(ret, exit);

    if (outCount)
    {
        *outCount = payload_length;
    }

exit:
    return ret;
}

int UCIprotoWrite(
                const uint8_t inType,
                const uint8_t inGID,
                const uint8_t inOID,
                const uint8_t *inData,
                const int inCount)
{
    int ret = -EINVAL;
    uint8_t *header;
    uint8_t *payload;

    if (inCount)
    {
        require(inData, exit);
    }
    else
    {
        require(inData == NULL, exit);
    }

    require((inCount + UCI_MSG_HDR_SIZE) < sizeof(mUCI.txbuf), exit);

#if DUMP_PROTO
    uci_dump("TX->", inType, inGID, inOID, inData, inCount);
#endif
    header  = mUCI.txbuf;
    payload = mUCI.txbuf + UCI_MSG_HDR_SIZE;

    header[0] = (inType << UCI_MT_SHIFT) | ((inGID << UCI_GID_SHIFT) & UCI_GID_MASK);
    header[1] = (inOID << UCI_OID_SHIFT) & UCI_OID_MASK;
    header[2] = 0; // RFU
    header[3] = inCount & 0xff; // will be filled out in txCommand

    if (inCount)
    {
        memcpy(payload, inData, inCount);
    }

    mUCI.txcnt = UCI_MSG_HDR_SIZE + inCount;

    ret = _UCItxCommand();
exit:
    return ret;
}

static int _UCIidleProcess(void)
{
    int ret = -EINVAL;

    require(mUCI.state == UCI_IDLE, exit);

    if (mUCI.needs_reset)
    {
        mUCI.state = UCI_RESET;
        mUCI.nextstate = UCI_RESET;
    }
    else if (!mUCI.got_dev_info)
    {
        mUCI.state = UCI_RX;
        mUCI.nextstate = UCI_IDLE;
        mUCI.cmd_start = k_uptime_get();
        // send get-device-info
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_DEVICE_INFO, NULL, 0);
    }
    else if (!mUCI.got_cap_info)
    {
        mUCI.state = UCI_RX;
        mUCI.nextstate = UCI_IDLE;
        mUCI.cmd_start = k_uptime_get();
        // send get-cap-info
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_GET_CAPS_INFO, NULL, 0);
    }
    else
    {
        ret = 0;
    }
exit:
    return ret;
}

int UCIprotoSlice(uint32_t *delay)
{
    int ret = 0;
    bool readable;
    uint8_t data[32];
    uint64_t now;

    ret = NRFSPIpoll(&readable);
    require_noerr(ret, exit);

    if (readable)
    {
        int count;
        uint8_t type;
        uint8_t gid;
        uint8_t oid;

        ret = UCIprotoRead(&type, &gid, &oid, mUCI.rxbuf, sizeof(mUCI.rxbuf), &count);

        if (ret >= 0)
        {
            mUCI.timeout_count = 0;
#if DUMP_PROTO
            uci_dump("<-RX", type, gid, oid, mUCI.rxbuf, count);
#endif
            uci_decode(type, gid, oid, mUCI.rxbuf, count);
            ret = 0;
        }
    }

    switch (mUCI.state)
    {
    case UCI_BOOT:
        // wait for init state ntf from UWBS
        break;

    case UCI_INIT:
        // send proprietary set-device state to nxp rhodes v4 and get respone
        mUCI.state = UCI_RX;
        mUCI.nextstate = UCI_OFFLINE;
        mUCI.cmd_start = k_uptime_get();
        data[0] = 0x73;
        data[1] = 0x04;
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_PROPRIETARY, EXT_UCI_MSG_CORE_DEVICE_INIT, data, 2);
        *delay = 2;
        break;

    case UCI_OFFLINE:
        // wait for ok state ntf after proprietary init or reset was sent
        *delay = 100;
        break;

    case UCI_RESET:
        // send a reset. when the ok status ntf comes back, it will set idle state
        mUCI.state = UCI_RX;
        mUCI.nextstate = UCI_OFFLINE;
        mUCI.cmd_start = k_uptime_get();
        data[0] = 0;
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_DEVICE_RESET, data, 1);
        *delay = 2;
        break;

    case UCI_IDLE:
        ret = _UCIidleProcess();
        *delay = 100;
        break;

    case UCI_TX: /* retransmit */
        if (mUCI.txcnt > 0)
        {
            mUCI.state = UCI_RX;
            mUCI.nextstate = UCI_IDLE;
            _UCItxCommand();
        }
        else
        {
            LOG_ERR("No command to re-transmit?");
            mUCI.state = UCI_IDLE;
        }
        *delay = 10;
        break;

    case UCI_RX:
        now = k_uptime_get();
        if ((now - mUCI.cmd_start) > UCI_RESP_TIMEOUT)
        {
            LOG_WRN("Resp timeout going to state %d", mUCI.nextstate);
            mUCI.timeout_count++;
            /* TODO - RESET? */
            mUCI.state = mUCI.nextstate;
        }
        *delay = 10;
        break;

    default:
        break;
    }

exit:
    return ret;
}

int UCIprotoInit(bool inWarmStart)
{
    int ret = 0;

    memset(&mUCI, 0, sizeof(mUCI));

    mUCI.state = inWarmStart ? UCI_INIT : UCI_BOOT;
    mUCI.nextstate = UCI_INIT;
    mUCI.timeout_count = 0;
    mUCI.max_packet = UCI_MAX_PAYLOAD_SIZE;
    mUCI.rxcnt = 0;
    mUCI.txcnt = 0;

    return ret;
}

