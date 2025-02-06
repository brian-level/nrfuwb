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
        UCI_DEV_INFO,
        UCI_CAP_INFO,
        UCI_IDLE,
        UCI_ACTIVE,
        UCI_RESP,
    }
    state, prevstate, nextstate;

    uint64_t cmd_start;
    uint32_t timeout_count;

    uint8_t rxbuf[255];
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
    case UCI_DEV_INFO:  statestr = "DEV_INFO"; break;
    case UCI_CAP_INFO:  statestr = "CAP_INFO"; break;
    case UCI_IDLE:      statestr = "IDLE"; break;
    case UCI_ACTIVE:    statestr = "ACTIVE"; break;
    case UCI_RESP:      statestr = "RESPONSE"; break;
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
                        // "init, do a proprietary init sequence"
                        //
                        if (mUCI.state == UCI_BOOT)
                        {
                            LOG_INF("UWB Device Status Init, Booting");
                            mUCI.state = UCI_INIT;
                        }
                    }
                    else if (inData[0] == 1)
                    {
                        LOG_INF("UWB Device Ready, ps=%d", mUCI.prevstate);

                        if (mUCI.state == UCI_OFFLINE)
                        {
                            if (mUCI.prevstate == UCI_INIT)
                            {
                                // finished proprietary init, do a dev reset
                                //
                                mUCI.state = UCI_RESET;
                            }
                            else if (mUCI.prevstate == UCI_RESET)
                            {
                                mUCI.state = UCI_DEV_INFO;
                            }
                            else
                            {
                                mUCI.state = UCI_IDLE;
                            }
                        }
                    }
                    else if (inData[0] == 2)
                    {
                        LOG_INF("UWB Device Active");
                        mUCI.state = UCI_ACTIVE;
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
                    LOG_WRN("Restart command in state %d", mUCI.prevstate);
                    // repeat command
                    mUCI.state = mUCI.prevstate;
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
                break;
            case UCI_MSG_CORE_DEVICE_STATUS_NTF:
            case UCI_MSG_CORE_DEVICE_INFO:
            case UCI_MSG_CORE_GET_CAPS_INFO:
            case UCI_MSG_CORE_SET_CONFIG:
            case UCI_MSG_CORE_GET_CONFIG:
            case UCI_MSG_CORE_DEVICE_SUSPEND:
                break;
            case UCI_MSG_CORE_GENERIC_ERROR_NTF:
                LOG_WRN("Error response, restarting");
                // repeat command
                mUCI.state = UCI_RESET;
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
                // we can do a normal startupwhen we get an ok status ntf
                //
                if (inCount > 0 && inData[0] == 0)
                {
                    LOG_INF("UWB pre-Init OK");
                    mUCI.state = UCI_OFFLINE;
                }
            }
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
    int ret = -1;
    uint8_t header[4];
    int chunk;
    int total = 0;
    uint8_t frag;
    int max_packet = 255;

    if (inCount)
    {
        require(inData, exit);
    }
    else
    {
        require(inData == NULL, exit);
    }

    total = 0;

#if DUMP_PROTO
    uci_dump("TX->", inType, inGID, inOID, inData, inCount);
#endif
    do
    {
        chunk = inCount - total;
        frag = 0;
        if (chunk > max_packet)
        {
            chunk = max_packet;
            frag = UCI_GID_MASK;
        }

        header[0] = (inType << UCI_MT_SHIFT) | frag | ((inGID << UCI_GID_SHIFT) & UCI_GID_MASK);
        header[1] = (inOID << UCI_OID_SHIFT) & UCI_OID_MASK;
        header[2] = 0; // RFU
        header[3] = chunk;

        // xfer header
        ret = NRFSPIwrite(header, 4);

        if (chunk)
        {
            k_sleep(K_USEC(80));

            // xfer chunk
            ret = NRFSPIwrite(inData, chunk);

            total += chunk;
            inData += chunk;
        }
    }
    while (total < inCount);

    ret = 0;
exit:
    return ret;
}

int UCIprotoSlice(uint32_t *delay)
{
    int ret;
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
        break;

    case UCI_INIT:
        // send proprietary set-device state to nxp rhodes v4
        data[0] = 0x73;
        data[1] = 0x04;
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_PROPRIETARY, EXT_UCI_MSG_CORE_DEVICE_INIT, data, 2);
        // wait for response (with timeout)
        mUCI.prevstate = UCI_INIT;
        mUCI.state = UCI_RESP;
        // and go to offline, waiting for ready status
        mUCI.nextstate = UCI_OFFLINE;
        mUCI.cmd_start = k_uptime_get();
        *delay = 0;
        break;

    case UCI_RESET:
        // send device-reset
        data[0] = 0;
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_DEVICE_RESET, data, 1);
        // wait for response (with timeout)
        mUCI.prevstate = UCI_RESET;
        mUCI.state = UCI_RESP;
        // when a response comes back, wait for ready in offline state
        mUCI.nextstate = UCI_OFFLINE;
        mUCI.cmd_start = k_uptime_get();
        *delay = 0;
        break;

    case UCI_DEV_INFO:
        // send get-device-info
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_DEVICE_INFO, NULL, 0);
        // wait for response (with timeout)
        mUCI.prevstate = UCI_DEV_INFO;
        mUCI.state = UCI_RESP;
        mUCI.nextstate = UCI_CAP_INFO;
        mUCI.cmd_start = k_uptime_get();
        *delay = 0;
        break;

    case UCI_CAP_INFO:
        // send get-device-info
        ret = UCIprotoWrite(UCI_MT_CMD, UCI_GID_CORE, UCI_MSG_CORE_GET_CAPS_INFO, NULL, 0);
        // wait for response (with timeout)
        mUCI.prevstate = UCI_CAP_INFO;
        mUCI.state = UCI_RESP;
        mUCI.nextstate = UCI_IDLE;
        mUCI.cmd_start = k_uptime_get();
        *delay = 0;
        break;

    case UCI_OFFLINE:
        *delay = 10;
        break;

    case UCI_IDLE:
        *delay = 100;
        break;

    case UCI_RESP:
        now = k_uptime_get();
        if ((now - mUCI.cmd_start) > UCI_RESP_TIMEOUT)
        {
            LOG_WRN("Resp timeout in state %d", mUCI.prevstate);
            mUCI.timeout_count++;
            mUCI.state = mUCI.prevstate;
        }
        *delay = 1;
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

    mUCI.state = inWarmStart ? UCI_INIT : UCI_BOOT;
    mUCI.prevstate = mUCI.state;
    mUCI.nextstate = UCI_INIT;
    mUCI.timeout_count = 0;

    return ret;
}

