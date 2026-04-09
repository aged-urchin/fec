#ifndef ___TIMEUTILS_H___
#define ___TIMEUTILS_H___

#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <time.h>
#else /// WEBRTC_WIN
#include <sys/time.h>
#include <unistd.h>
#if defined(WEBRTC_MAC)  /// mac & ios
#include <mach/mach_time.h>
#endif
#endif

#include <cstdint>
#include <string>

#if defined(_MSC_VER) && defined(_WIN32)
#pragma comment(lib, "winmm.lib")
#endif

/** 1 second (s) =
 *  1000 milliseconds (ms) =
 *  1000 * 1000 microseconds (us) =
 *  1000 * 1000 * 1000 nanoseconds (ns) =
 *  1000 * 1000 * 1000 * 1000 piscoseconds (ps)
 */
///< #define kPreferClockRealtime 0 ///< ios crash if opened
static const int64_t kNtpUtcDiffSecs        = 0x83AA7E80; ///< ntp time = utc time + 0x83AA7E80
static const int64_t kMilliSecondsPerSecond = 1000;
static const int64_t kMicroSecondsPerSecond = 1000 * 1000;
static const int64_t kNanoSecondsPerSecond  = 1000 * 1000 * 1000;

#define NANO2MILLI(nano)            (nano * kMilliSecondsPerSecond / kNanoSecondsPerSecond)
#define MILLI2NANO(ms)              (ms * kNanoSecondsPerSecond / kMilliSecondsPerSecond)

#if defined(_WIN32)
static volatile bool                    g_s_tlsFlag       = false;
static unsigned long                    g_s_tlsKey0       = (unsigned long)-1;
static unsigned long                    g_s_tlsKey1       = (unsigned long)-1;
static int64_t                          g_s_globalTick    = 0;
#elif defined(WEBRTC_MAC)  /// mac & ios
static volatile bool                    g_s_timebaseFlag  = false;
static mach_timebase_info_data_t        g_s_timebaseInfo  = { 0, 0 };
#endif
static std::mutex                       g_timeutil_mutex;

class Time
{
public:
    static void sleep(uint32_t milliseconds)
    {
        /** under ideal conditions is accurate to one microsecond. To get nanosecond
         *  accuracy, replace sleep()/usleep() with something with higher resolution
         *  like nanosleep() or ppoll().
         */
        if (0 == milliseconds)
        {
#if defined(_WIN32)
            ::Sleep(0);
#else
            usleep(0);
#endif
            return;
        }

        if ((uint32_t)-1 == milliseconds)
        {
            while (true)
            {
#if defined(_WIN32)
                ::Sleep(-1);
#else
                usleep(999999);
#endif
            }
        }

        const int64_t te = tickcount() + milliseconds;

        while (true)
        {
#if defined(_WIN32)
            ::Sleep(1);
#else
            usleep(500);
#endif
            if (tickcount() >= te)
            {
                break;
            }
        }
    }

    /** provides a monotonic time(e.g. system uptime) in milliseconds
     *  Monotonic time is useful for measuring elapsed times,
     *  because it guarantees that those measurements are not affected by changes to the system clock.
     */
    static int64_t tickcount()
    {
#if defined(_WIN32)
        if (!g_s_tlsFlag)
        {
            g_timeutil_mutex.lock();
            if (!g_s_tlsFlag) /* double check */
            {
                g_s_tlsKey0    = ::TlsAlloc(); /* dynamic TLS */
                g_s_tlsKey1    = ::TlsAlloc(); /* dynamic TLS */
                g_s_globalTick = ::timeGetTime();

                g_s_tlsFlag = true;
            }
            g_timeutil_mutex.unlock();
        }

        bool updateGlobalTick = false;

        uint32_t tick0 = (uint32_t)(uint64_t)::TlsGetValue(g_s_tlsKey0);
        uint32_t tick1 = (uint32_t)(uint64_t)::TlsGetValue(g_s_tlsKey1);
        if (tick0 == 0 && tick1 == 0)
        {
            g_timeutil_mutex.lock();
            tick0 = (uint32_t)g_s_globalTick;
            tick1 = (uint32_t)(g_s_globalTick >> 32);
            g_timeutil_mutex.unlock();

            ::TlsSetValue(g_s_tlsKey0, (void*)(uint64_t)tick0);
            ::TlsSetValue(g_s_tlsKey1, (void*)(uint64_t)tick1);

            updateGlobalTick = true;
        }

        const uint32_t tick = ::timeGetTime();
        if (tick > tick0)
        {
            tick0 = tick;
            ::TlsSetValue(g_s_tlsKey0, (void*)(uint64_t)tick0);
        }
        else if (tick < tick0)
        {
            tick0 = tick;
            ++tick1;
            ::TlsSetValue(g_s_tlsKey0, (void*)(uint64_t)tick0);
            ::TlsSetValue(g_s_tlsKey1, (void*)(uint64_t)tick1);

            updateGlobalTick = true;
        }
        else
        {
        }

        int64_t ret = tick1;
        ret <<= 32;
        ret |=  tick0;

        if (updateGlobalTick)
        {
            g_timeutil_mutex.lock();
            if (ret > g_s_globalTick)
            {
                g_s_globalTick = ret;
            }
            g_timeutil_mutex.unlock();
        }

        return (ret);
#elif defined(WEBRTC_MAC)/// mac & ios
        if (!g_s_timebaseFlag)
        {
            g_timeutil_lock.Lock();
            if (!g_s_timebaseFlag) /* double check */
            {
                mach_timebase_info(&g_s_timebaseInfo);

                g_s_timebaseFlag = true;
            }
            g_timeutil_lock.Unlock();
        }

        int64_t ret = mach_absolute_time();
        ret = ret * g_s_timebaseInfo.numer / g_s_timebaseInfo.denom; /* ns_ticks ---> ns */
        ret = NANO2MILLI(ret);                                       /* ns       ---> ms */

        return ret;
#else
        struct timespec ts;
        /** Each clock has its own resolution and epoch. You can find the resolution of a clock with the function clock_getres.
         *  There is no function to get the epoch for a clock; either it is fixed and documented, or the clock is not meant to be used to measure absolute times.
         */
        clock_gettime(CLOCK_MONOTONIC, &ts);

        int64_t ret = ts.tv_sec;

        ret *= kMilliSecondsPerSecond;
        ret += NANO2MILLI(ts.tv_nsec);

        return ret;
#endif
    }

    static int64_t clocktime()
    {
        /** milliseconds of calendar time elapsed since the POSIX epoch (1/1/1970 00:00:00 UTC)
         */
        UTCTime time = get_utc_time();
        /** 'tv_sec' & 'tv_nsec' are of types 'long', which is 32bit on arm.
         *  we must cast 'tv_sec' into int64_t for safety.
         */
        return (int64_t)time.tv_sec * kMilliSecondsPerSecond + NANO2MILLI(time.tv_nsec);
    }

    static std::string clocktime_s()
    {
        /** return the formatted UTC time string
         *  !!! for display, use a more readable wallclock time.
         *      (this time could be changed by NTP or user)
         */
        auto time = clocktime();

        time_t curtime = time / kMilliSecondsPerSecond;
        struct tm *timeinfo = localtime(&curtime);

        char time_string[64] = { 0 };
        sprintf(time_string, "%.2d-%.2d %.2d:%.2d:%.2d:%03d",
                timeinfo->tm_mon + 1,
                timeinfo->tm_mday,
                timeinfo->tm_hour,
                timeinfo->tm_min,
                timeinfo->tm_sec,
                (int)(time % kMilliSecondsPerSecond));

        return time_string;
    }

    /** time since 1/1/1900 00:00:00 UTC expressed in
     *  32bit MSB(in seconds) + 32bit LSB(in 232 picoseconds)
     */
    static uint64_t ntp_from_utc(int64_t utc_ms)
    {
        UTCTime time = { static_cast<time_t>(utc_ms / kMilliSecondsPerSecond), static_cast<long>(utc_ms % kMilliSecondsPerSecond * kMicroSecondsPerSecond) };

        /** convert nanoseconds to 32-bits fraction (232 picoseconds units, that is (1e12 / (1 + UINT_MAX)))
         */
        uint64_t t = (uint64_t)(time.tv_nsec) << 32u;
        t /= kNanoSecondsPerSecond;

        /** There is 70 years (incl. 17 leap ones) offset to the Unix Epoch.
         *  No leap seconds during that period since they were not invented yet.
         */
        ///< assert (t < 0x100000000);
        t |= (uint64_t)((70LL * 365 + 17) * 24 * 60 * 60 + time.tv_sec) << 32u;

        /** same as below:
         *  uint64_t msw = time.tv_sec + 0x83AA7E80; ///< 0x83AA7E80 is the number of seconds from 1900 to 1970
         *  uint64_t lsw = (uint32_t)((double)time.tv_nsec * (double)(((uint64_t)1) << 32) * 1.0e-9);
         *  t = msw << 32 | lsw;
         */
        return t;
    }

    static int64_t ntp_ms_to_utc_ms(int64_t ntp_ms)
    {
        return ntp_ms - kNtpUtcDiffSecs * kMilliSecondsPerSecond;
    }

    static int64_t utc_ms_to_ntp_ms(int64_t utc_ms)
    {
        return utc_ms + kNtpUtcDiffSecs * kMilliSecondsPerSecond;
    }

private:

    struct UTCTime
    {
        time_t  tv_sec;     /** seconds  **/
        long    tv_nsec;    /** and the fraction(in nanoseconds (1e-9) (1000 * microseconds (1e-6))) **/
    };

    static UTCTime get_utc_time()
    {
        /** get time expressed as the amount of time since the POSIX Epoch (1/1/1970 00:00:00 UTC)
         */
        struct UTCTime time = { 0, 0 };

#if defined(_WIN32)
        uint64_t intervals;
        FILETIME ft;

        GetSystemTimeAsFileTime(&ft);

        /**
         * A file time is a 64-bit value that represents the number
         * of 100-nanosecond intervals that have elapsed since
         * January 1, 1601 12:00 A.M. UTC.
         *
         * Between January 1, 1970 (Epoch) and January 1, 1601 there were
         * 134744 days,
         * 11644473600 seconds or
         * 11644473600,000,000,0 100-nanosecond intervals.
         */
        intervals = ((uint64_t) ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        intervals -= 116444736000000000;

        time.tv_sec  = (long)(intervals / 10000000);
        time.tv_nsec = ((long)((intervals % 10000000) / 10)) * 1000;
#elif defined (CLOCK_REALTIME) && defined (kPreferClockRealtime)
        {
            /** CLOCK_REALTIME : the system's real time (i.e. wall time) clock, expressed as the amount of time since the POSIX Epoch, 00:00:00 on January 1, 1970, Coordinated Universal Time.
             *                   This is the same as the value returned by gettimeofday(2).
             */
            struct timespec ts;
            clock_gettime (CLOCK_REALTIME, &ts);

            time.tv_sec  = ts.tv_sec;
            time.tv_nsec = ts.tv_nsec;
        }
#else
        {
            /** !!! avoid the use of 'gettimeofday' to measure time
             *  https://blog.habets.se/2010/09/gettimeofday-should-never-be-used-to-measure-time.html
             */

            struct timeval tv;
            /** get the current calendar time
             */
            gettimeofday (&tv, nullptr);

            time.tv_sec  = tv.tv_sec;
            time.tv_nsec = tv.tv_usec * (kNanoSecondsPerSecond / kMicroSecondsPerSecond);
        }
#endif

        return time;
    }
};

#endif ///< ___TIMEUTILS_H___
