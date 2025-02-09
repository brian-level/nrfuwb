
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#define COMPONENT_NAME uwbmain
#include "Logging.h"

#if CONFIG_BT
    #include "level_ble.h"
#endif
#include "timesvc.h"
#include "crypto_platform.h"
#include "uci_proto.h"
#include "hbci_proto.h"
#include "nrfspi.h"

int main( void )
{
    int ret;
    bool warmStart;

    LOG_INF("NRF UWB Device");

    uint32_t delay = 2000;
    uint32_t min_delay;

    ret = CryptoPlatformInit();
    require_noerr(ret, exit);

    ret = TimeInit();
    require_noerr(ret, exit);

    ret = NRFSPIinit();
    require_noerr(ret, exit);

    ret = HBCIprotoInit(&warmStart);
    require_noerr(ret, exit);

    ret = UCIprotoInit(warmStart);
    require_noerr(ret, exit);

#if CONFIG_BT
    ret = BLEinit(CONFIG_BT_DEVICE_NAME);
    require_noerr(ret, exit);
#endif
    // Run the application
    //
    while (true)
    {
        min_delay = delay;
        delay = 200;

        #if CONFIG_BT
        ret = BLEslice(&delay);
        if (delay < min_delay)
        {
            min_delay = delay;
        }
        #endif

        ret = UCIprotoSlice(&delay);
        if (delay < min_delay)
        {
            min_delay = delay;
        }

        // sleep for up-to min_delay.  Any events that need attention
        // will call TimeSignalApplicationEvent causing this function
        // to return immediately
        //
        TimeWaitApplicationEvent(min_delay);
    }

exit:
    return ret;
}

void Logging_CallAssertionHandlerLite(
        const char *    functionName,
        long            lineNumber,
        long            errorCode )
{
    LOG_ERR("Assert %s:%ld  %ld", functionName, lineNumber, errorCode);
}

void Logging_DebugAssert(
        const char *    componentNameString,
        const char *    assertionString,
        const char *    exceptionLabelString,
        const char *    errorString,
        const char *    fileName,
        const char *    functionName,
        long            lineNumber,
        long            errorCode )
{
    LOG_ERR("Assert %s %s:%ld  %ld", functionName, fileName, lineNumber, errorCode);
}

