#include "uwb.h"
#include "uwb_defs.h"
#include "hbci_proto.h"
#include "uci_proto.h"
#include "uci_defs.h"
#include "uci_ext_defs.h"
#include "nrfspi.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#define COMPONENT_NAME uwb
#include "Logging.h"

// Define this non-0 to dump protocol bytes
#define DUMP_PROTO 1

#define UCI_RESP_TIMEOUT    (30)

static struct
{
    bool initialized;
}
mUWB;


int UWBRead(
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

int UWBWrite(
                const uint8_t *inData,
                const int inCount)
{
    int ret = -EINVAL;

    require(inData, exit);
    require(inCount, exit);
    ret = 0;
exit:
    return ret;
}

int UWBSlice(uint32_t *delay)
{
    int ret= 0;

    ret = UCIprotoSlice(delay);
//exit:
    return ret;
}

int UWBinit(void)
{
    int ret = 0;
    bool warmStart;

    ret = NRFSPIinit();
    require_noerr(ret, exit);

    ret = HBCIprotoInit(&warmStart);
    require_noerr(ret, exit);

    ret = UCIprotoInit(warmStart);
    require_noerr(ret, exit);

    mUWB.initialized = true;
exit:
    return ret;
}

