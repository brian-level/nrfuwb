
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisec;
}
date_time_t;

typedef enum
{
    TIME_FORMAT_12H,
    TIME_FORMAT_24H,
    TIME_FORMAT_RFC3339
}
date_format_t;

uint64_t    TimeUptimeMilliseconds( void );
uint32_t    TimeUptimeSeconds( void );
uint32_t    TimeEpochSeconds( void );
int         TimeSetEpochSeconds( uint32_t inSeconds );
int         TimeWaitApplicationEvent( uint32_t inDelay );
void        TimeSignalApplicationEvent( void );
int         TimeInit( void );

const char *TimeMonthAsString( int inMonth );
const char *TimeDayAsString( int inDay );
int         TimeDayOfWeek( int inDay, int inMonth, int inYear );
uint32_t    TimeDaysInYear( int inYear );
uint32_t    TimeDaysInMonth( int inMonth, int inYear );

void        TimeTimeToDateTime(const uint32_t inEpochSeconds, date_time_t *outDate);
void        TimeDateTimeToTime(const date_time_t *inDate, uint32_t *outEpochSeconds);

int         TimeDateAsString(uint32_t inEpochSeconds, bool inIncludeDay, char * outBuf, int outBufSize);
int         TimeAsString(uint32_t inEpochSeconds, date_format_t inFormat, char * outBuf, int outBufSize);
int         DateAndTimeAsString(uint32_t inEpochSeconds, date_format_t inFormat, char * buf, int bufsize, bool inIncludeDay);
