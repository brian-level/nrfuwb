
#include "timesvc.h"

#define COMPONENT_NAME timesvc
#include "Logging.h"

#define EPOCH_YEAR_ZERO     (1970)

#define SECONDS_PER_MINUTE  (60)
#define SECONDS_PER_HOUR    (60 * 60)
#define SECONDS_PER_DAY     (24 * SECONDS_PER_HOUR)

#include <hal/nrf_clock.h>
#include <hal/nrf_rtc.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>
#include <zephyr/shell/shell.h>

#include <stdlib.h>

#ifdef CONFIG_SOC_NRF5340_CPUAPP
// only 2 rtcs in 5340, so use 0, since 1 is used for zephyr
#define RTC     (NRF_RTC0)
#define RTC_IRQ (RTC0_IRQn)
#else
// first 2 rcts used for soft-app and zephyr
#define RTC     (NRF_RTC2)
#define RTC_IRQ (RTC2_IRQn)
#endif

static struct
{
    int64_t epoch;      // Unix time in microseconds (compensated)
    int64_t rawsecs;    // Unix time in microseconds (uncompensated)
    int64_t drift;      // microseconds per second +/- drift
    int64_t last_set;   // Time when last set
    struct k_sem event;
}
mTime;

ISR_DIRECT_DECLARE(_RTC_ISR)
{
    static uint32_t irq_counter;

    nrf_rtc_event_clear(RTC, NRF_RTC_EVENT_TICK);

    irq_counter++;
    if (!(irq_counter & 0x7))
    {
        // every 8 IRQs is one second
        mTime.epoch+= mTime.drift;
        mTime.rawsecs++;
    }

    return 0;
}

uint64_t TimeUptimeMilliseconds( void )
{
    uint64_t now = k_uptime_get();

    return now;
}

uint32_t TimeUptimeSeconds( void )
{
    return (uint32_t)((k_uptime_get() + 500) / 1000);
}

uint32_t TimeEpochSeconds( void )
{
    return (uint32_t)(mTime.epoch / 1000000);
}

int TimeSetEpochSeconds( uint32_t inSeconds )
{
    int64_t newtime;
    bool time_was_reset = false;

    newtime = (int64_t)inSeconds * 1000000;
    mTime.drift = 1000000;

    if (mTime.epoch > 1675380264)
    {
        int64_t delta;
        int64_t adelt;
        int64_t microsecs_since;

        // what we have for time is later than 11:30am 2 Feb 2023
        // so delta might be resonable
        //
        delta = newtime - mTime.epoch;

        // get abs(delta) between our compensated time and what the new time is
        // it should be close to 0 if we're compensated properly
        //
        adelt = delta;
        if (adelt < 0)
        {
            adelt = -adelt;
        }

        if (adelt < (SECONDS_PER_MINUTE * 1000000))
        {
            int64_t drift;

            if (adelt > 1000000)
            {
                // off by more than a second, display new time
                time_was_reset = true;
            }
            // we're within a minute of the time we're being set to
            // so form a drift offset (units are 1 + (microseconds per second))
            // between our raw time and new time
            //
            delta = newtime - (1000000 * mTime.rawsecs);
            microsecs_since = newtime - mTime.last_set;
            drift = (delta / microsecs_since) / 1000000;
            mTime.drift = 1000000 + drift;
        }
        else
        {
            time_was_reset = true;
        }
    }
    else
    {
        time_was_reset = true;
    }

    mTime.last_set = newtime;
    mTime.epoch    = newtime;
    mTime.rawsecs  = inSeconds;

    if (time_was_reset)
    {
        char tbuf[32];
        char dbuf[32];
        uint32_t now;

        now = TimeEpochSeconds();
        TimeDateAsString(now, true, dbuf, sizeof(dbuf));
        TimeAsString(now, TIME_FORMAT_12H,  tbuf, sizeof(tbuf));
        LOG_INF("Time: now %s %s  drift is %lld usecs/sec", dbuf, tbuf, mTime.drift - 1000000);
    }
    return 0;
}

int TimeWaitApplicationEvent( uint32_t inDelay )
{
    int result;

    result = k_sem_take(&mTime.event, K_MSEC(inDelay));

    // result != 0 if timed-out, which is OK
    return result;
}

void TimeSignalApplicationEvent( void )
{
    k_sem_give(&mTime.event);
}

#ifdef CONFIG_SHELL

static void _PrintDateTime(const struct shell *shell)
{
    uint32_t now;
    char datebuf[32];
    char timebuf[32];

    now = TimeEpochSeconds();
    TimeDateAsString(now, true, datebuf, sizeof(datebuf));
    TimeAsString(now, TIME_FORMAT_12H,  timebuf, sizeof(timebuf));
    shell_print(shell, "%s %s\n", datebuf, timebuf);
}

static int _CmdTimeInfo( const struct shell *shell, size_t argc, char **argv )
{
    shell_print(shell, "Time now=%lld  raw=%lld  drift=%lld",
                mTime.epoch, 1000000 * mTime.rawsecs, mTime.drift - 1000000);
    _PrintDateTime(shell);
    return 0;
}

static int _CmdTimeNow( const struct shell *shell, size_t argc, char **argv )
{
    _PrintDateTime(shell);
    return 0;
}

static int _CmdTimeSet( const struct shell *shell, size_t argc, char **argv )
{
    uint32_t now;

    now = strtoul(*++argv, NULL, 0);
    TimeSetEpochSeconds(now);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_Timesvc,
    SHELL_CMD(info, NULL,    " Print info about time\n", _CmdTimeInfo),
    SHELL_CMD(now, NULL,     " Print current time\n", _CmdTimeNow),
    SHELL_CMD_ARG(set, NULL, " Set current time (use time set <epochseconds>\n", _CmdTimeSet, 1, 1),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(time, &sub_Timesvc, "Time Services", NULL);

#endif

int TimeInit( void )
{
    int err = -1;

    mTime.epoch = 1675357200; // ground hog day 2023
    mTime.rawsecs = 0;
    mTime.drift = 1000000;

    err = k_sem_init(&mTime.event, 1, 1);
    require_noerr( err, exit );

    // clock is already started in system clock init by zephyr
    // z_nrf_clock_control_lf_on(CLOCK_CONTROL_NRF_LF_START_NOWAIT);

    // Set prescaler to the max value (12-bit) giving an 8Hz counter frequency (32768/4096)
    nrf_rtc_prescaler_set(RTC, 0xFFF);

    nrf_rtc_event_enable(RTC, NRF_RTC_INT_TICK_MASK);
    nrf_rtc_int_enable(RTC, NRF_RTC_INT_TICK_MASK);
    nrf_rtc_task_trigger(RTC, NRF_RTC_TASK_START);

    // hook in RTC Interrupt
    IRQ_DIRECT_CONNECT(RTC_IRQ, IRQ_PRIO_LOWEST, _RTC_ISR, 0);
    irq_enable(RTC_IRQ);

exit:
    return err;
}

static const uint8_t s_days_in_month[12] = {
    31,29,31,30,31,30,31,31,30,31,30,31
};

const char *TimeMonthAsString(int inMonth)
{
    switch (inMonth)
    {
    case 1:     return "Jan";
    case 2:     return "Feb";
    case 3:     return "Mar";
    case 4:     return "Apr";
    case 5:     return "May";
    case 6:     return "Jun";
    case 7:     return "Jul";
    case 8:     return "Aug";
    case 9:     return "Sep";
    case 10:    return "Oct";
    case 11:    return "Nov";
    case 12:    return "Dec";
    default:    return "???";
    }
}

const char *TimeDayAsString(int inDay)
{
    switch (inDay)
    {
    case 1:     return "Mon";
    case 2:     return "Tue";
    case 3:     return "Wed";
    case 4:     return "Thu";
    case 5:     return "Fri";
    case 6:     return "Sat";
    case 0: // fall through
    case 7:     return "Sun";
    default:    return "???";
    }
}

int TimeDayOfWeek(int inDay, int inMonth, int inYear)
{
    int cent;
    int dow;

    // from rfc3339
    //
    inMonth -= 2;
    if (inMonth < 1)
    {
       inMonth += 12;
       --inYear;
    }

    cent = inYear / 100;
    inYear %= 100;
    dow = ((26 * inMonth - 2) / 10 + inDay + inYear + inYear / 4 + cent / 4 + 5 * cent) % 7;

    if (dow == 0)
    {
        dow = 7;
    }

    return dow;
}

uint32_t TimeDaysInYear(int inYear)
{
    // every 4th year is a leap year
    //
    if (inYear & 0x3)
    {
        return 365;
    }
    return 366;
}

uint32_t TimeDaysInMonth(int inMonth, int inYear)
{
    if (inMonth == 2)
    {
        return (inYear & 0x03) ? 28 : 29;
    }

    if (inMonth < 13)
    {
        return s_days_in_month[inMonth - 1];
    }

    return 0;
}

uint32_t TimeDayCountFromDate(date_time_t *inDate)
{
    uint32_t year;
    uint32_t month;
    uint32_t days;

    days = 0;

    for (year = EPOCH_YEAR_ZERO; year < (uint32_t)inDate->year; year++)
    {
        days += TimeDaysInYear(year);
    }

    for (month = 1; month < (uint32_t)inDate->month; month++)
    {
        days += TimeDaysInMonth(month, inDate->year);
    }

    days += inDate->day - 1;

    return days;
}

void TimeDayCountToDate(uint32_t inDays, date_time_t *outDate)
{
    uint32_t year;
    uint32_t month;
    uint32_t ourdays;
    uint32_t daycount;

    ourdays = 0;

    if (outDate)
    {
        for (year = EPOCH_YEAR_ZERO; ourdays < inDays; year++)
        {
            daycount = TimeDaysInYear(year);
            if ((ourdays + daycount) > inDays)
            {
                break;
            }

            ourdays += daycount;
        }

        outDate->year = year;

        for (month = 1; ourdays < inDays; month++)
        {
            daycount = TimeDaysInMonth(month, year);
            if ((ourdays + daycount) > inDays)
            {
                break;
            }
            ourdays += daycount;
        }

        outDate->month = month;
        outDate->day = inDays - ourdays + 1;
    }
}

void TimeTimeToDateTime(const uint32_t inEpochSeconds, date_time_t *outDate)
{
    uint32_t days;
    uint32_t seconds;

    if (outDate)
    {
        // see POSIX time 4.15
        //
        days = inEpochSeconds / SECONDS_PER_DAY;

        TimeDayCountToDate(days, outDate);

        seconds = inEpochSeconds - (days * SECONDS_PER_DAY);

        outDate->hour = seconds / SECONDS_PER_HOUR;
        seconds -= outDate->hour * SECONDS_PER_HOUR;

        outDate->minute = seconds / SECONDS_PER_MINUTE;
        seconds -= outDate->minute * SECONDS_PER_MINUTE;
        outDate->second = seconds;

        outDate->millisec = 0;
    }
}

void TimeDateTimeToTime(const date_time_t *inDate, uint32_t *outEpochSeconds)
{
    uint32_t epoch_year;
    uint32_t epoch_seconds;

    epoch_year = inDate->year - 1900; // year since 1900 (must be >= 70!)

    // from https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_15
    //
    epoch_seconds = (inDate->second +
          inDate->minute * SECONDS_PER_MINUTE +
          inDate->hour   * SECONDS_PER_HOUR +
          (inDate->day - 1) * SECONDS_PER_DAY +
          (epoch_year - 70) * 365    * SECONDS_PER_DAY +
          ((epoch_year - 69) / 4)    * SECONDS_PER_DAY -
          ((epoch_year - 1) / 100)   * SECONDS_PER_DAY +
          ((epoch_year + 299) / 400) * SECONDS_PER_DAY);

    if (outEpochSeconds)
    {
        *outEpochSeconds = epoch_seconds;
    }
}

int TimeDateAsString(uint32_t inEpochSeconds, bool inIncludeDay, char *buf, int bufsize)
{
    date_time_t theDate;
    const char *day = "";

    TimeTimeToDateTime(inEpochSeconds, &theDate);

    if (inIncludeDay)
    {
        day = TimeDayAsString(TimeDayOfWeek(theDate.day, theDate.month, theDate.year));
    }

    return snprintf(buf, bufsize, "%s%s%s %d, %d", day, *day ? " " : "",
                TimeMonthAsString(theDate.month), theDate.day, theDate.year);
}

int TimeAsString(uint32_t inEpochSeconds, date_format_t inFormat, char *buf, int bufsize)
{
    date_time_t theDate;
    const char *ampm;
    int hour;
    int num_chars_written = 0;

    TimeTimeToDateTime(inEpochSeconds, &theDate);

    switch (inFormat)
    {
    case TIME_FORMAT_12H:
        hour = theDate.hour;
        ampm = " am";
        if (hour >= 12)
        {
            if (hour > 12)
            {
                hour -= 12;
            }
            ampm = " pm";
        }
        num_chars_written = snprintf(buf, bufsize, "%02d:%02d:%02d%s", hour, theDate.minute, theDate.second, ampm);
        break;
    default:
    case TIME_FORMAT_24H:
        num_chars_written = snprintf(buf, bufsize, "%02d:%02d:%02d", theDate.hour, theDate.minute, theDate.second);
        break;
    case TIME_FORMAT_RFC3339:
        num_chars_written = snprintf(buf, bufsize, "T%02d:%02d:%02dZ", theDate.hour, theDate.minute, theDate.second);
        break;
    }

    return num_chars_written;
}

int DateAndTimeAsString(uint32_t inEpochSeconds, date_format_t inFormat, char * buf, int bufsize, bool inIncludeDay)
{
    int num_chars_written = TimeDateAsString(inEpochSeconds, inIncludeDay, buf, bufsize);
    buf[num_chars_written++] = ' ';
    num_chars_written += TimeAsString(inEpochSeconds, inFormat, &buf[num_chars_written], bufsize - num_chars_written);
    return num_chars_written;
}

