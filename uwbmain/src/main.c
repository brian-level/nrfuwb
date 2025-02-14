
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#define COMPONENT_NAME uwbmain
#include "Logging.h"

#if CONFIG_BT
    #include "level_ble.h"
#endif
#include "nearby_interaction.h"
#include "uwb.h"
#include "timesvc.h"
#include "crypto_platform.h"

int main( void )
{
    int ret;

    LOG_INF("NRF UWB Device");

    uint32_t delay = 2000;
    uint32_t min_delay;

    ret = CryptoPlatformInit();
    require_noerr(ret, exit);

    ret = TimeInit();
    require_noerr(ret, exit);

    ret = NIinit();
    require_noerr(ret, exit);

    #if CONFIG_BT
    ret = BLEinit(CONFIG_BT_DEVICE_NAME);
    require_noerr(ret, exit);
    #endif

    // Run the application
    //
    while (true)
    {
        delay = 200;
        min_delay = delay;

        ret = UWBSlice(&delay);
        if (delay < min_delay)
        {
            min_delay = delay;
        }

        #if CONFIG_BT
        if (UWBReady())
        {
            ret = BLEslice(&delay);
            if (delay < min_delay)
            {
                min_delay = delay;
            }
        }
        #endif

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

