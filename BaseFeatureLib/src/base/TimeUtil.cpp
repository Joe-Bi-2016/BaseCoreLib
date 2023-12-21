/*****************************************************************************
* Description: time util implemention.
* Author     : Bi ShengWang(shengwang.bisw@alibaba-inc.com.)
* Date       : 2020.05.28
* Copyright (c) alibaba All rights reserved.
******************************************************************************/
#include "../../inc/base/TimeUtil.h"
#include <time.h>
#include <chrono>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <windows.h>
#else
#include <sys/time.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
   #if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
   int gettimeofday(struct timeval* tp, void* tzp)
   {
        time_t clock;
        struct tm tm;
        SYSTEMTIME wtm;
        GetLocalTime(&wtm);
        tm.tm_year  = wtm.wYear - 1900;
        tm.tm_mon   = wtm.wMonth - 1;
        tm.tm_mday  = wtm.wDay;
        tm.tm_hour  = wtm.wHour;
        tm.tm_min  = wtm.wMinute;
        tm.tm_sec  = wtm.wSecond;
        tm.tm_isdst    = -1;
        clock  = mktime(&tm);
        tp->tv_sec = (long)clock;
        tp->tv_usec = wtm.wMilliseconds * 1000;
        return (0);
   }
   #endif

   //------------------------------------------------------------------------------------//
    long getCpuTickTimeOfMs(void)
    {
        return clock();
    }

   //------------------------------------------------------------------------------------//
    float diffCpuTimeOfSec(uint64 tick1, uint64 tick2)
    {
        return float(tick2 - tick1) / CLOCKS_PER_SEC;
    }

   //------------------------------------------------------------------------------------//
    float diffCpuTimeOfMs(uint64 tick1, uint64 tick2)
    {
        return float((tick2 - tick1) * PER_SEC_MSEC) / CLOCKS_PER_SEC;
    }

   //------------------------------------------------------------------------------------//
    float diffCpuTimeOfUs(uint64 tick1, uint64 tick2)
    {
        return float(tick2 - tick1);
    }

   //------------------------------------------------------------------------------------//
    uint64 getNowTimeOfSec(void)
    {
        return time(nullptr);
    }

   //------------------------------------------------------------------------------------//
    uint64 getNowTimeOfMs(void)
    {
        struct timeval t;
        gettimeofday(&t, nullptr);

        return uint64(t.tv_sec * PER_SEC_MSEC + t.tv_usec / PER_SEC_MSEC);
    }

   //------------------------------------------------------------------------------------//
    uint64 getNowTimeOfUs(void)
    {
        struct timeval t;
        gettimeofday(&t, nullptr);

        return uint64(t.tv_sec * PER_SEC_USEC + t.tv_usec);
    }

   //------------------------------------------------------------------------------------//
    uint64 getNowTimeOfNs(void)
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        auto now = std::chrono::system_clock::now();
        std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;
        return ns.count();
#else
        struct timespec t;
        clock_gettime(CLOCK_MONOTONIC, &t);
        return uint64(t.tv_sec * PER_SEC_NSEC + t.tv_nsec);
#endif
    }

   //------------------------------------------------------------------------------------//
    uint64 getNowSystemTimeOfSec(void)
    {
        auto now = std::chrono::system_clock::now();
        time_t time = std::chrono::system_clock::to_time_t(now);
        return time;
    }

    //------------------------------------------------------------------------------------//
    char* time2char(const time_t* t)
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        static char buff[64] = { 0 };
        memset(buff, 0x0, sizeof(buff));
        ctime_s(buff, sizeof(buff), t);
        return buff;
#else
        return ctime(t);
#endif
    }

   //------------------------------------------------------------------------------------//
    char* utcNowTime2String(void)
    {
        time_t now = time(nullptr);
        return time2char(&now);
    }

   //------------------------------------------------------------------------------------//
    char* localNowTime2String(void)
    {
        time_t now = time(nullptr);
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        tm local_t;
        static char buff[64] = { 0 };
        memset(buff, 0x0, sizeof(buff));
        localtime_s(&local_t, &now);
        asctime_s(buff, sizeof(buff), &local_t);
        return buff;
#else
        tm* local_t = localtime(&now);
        return asctime(local_t);
#endif
    }

   //------------------------------------------------------------------------------------//
    char* utcSecTime2String(uint64 t)
    {
        time_t now = time_t(t);
        return time2char(&now);
    }

   //------------------------------------------------------------------------------------//
    char* utcMSecTime2String(uint64 t)
    {
        time_t now = time_t(t / PER_SEC_MSEC);
        return time2char(&now);
    }

   //------------------------------------------------------------------------------------//
    char* utcMicrosTime2String(uint64 t)
    {
        time_t now = time_t(t / PER_SEC_USEC);
        return time2char(&now);
    }

__END__
