/*****************************************************************************
* FileName    : TimeUtil.h
* Description : Some time util function
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __TimeUtil_h__
#define __TimeUtil_h__
#include "Macro.h"
#include <string>

//---------------------------------------------------------------------------------------//
__BEGIN__
__CExternBegin__
    
   //------------------------------------------------------------------------------------//
    #define PER_SEC_MSEC    (1000LL)
    #define PER_SEC_USEC    (1000000LL)
    #define PER_SEC_NSEC    (1000000000LL)

   //------------------------------------------------------------------------------------//
    // get cpu current microsecond of process running time
    long getCpuTickTimeOfMs(void);

    // Note: t2 >= t1
    float diffCpuTimeOfSec(uint64 tick1, uint64 tick2); // diffuse of second with tick
    float diffCpuTimeOfMs(uint64 tick1, uint64 tick2);  // diffuse of millsecond with tick
    float diffCpuTimeOfUs(uint64 tick1, uint64 tick2);  // diffuse of microsecond with tick

    // return current time
    uint64 getNowTimeOfSec(void);
    uint64 getNowTimeOfMs(void);
    uint64 getNowTimeOfUs(void);
    uint64 getNowTimeOfNs(void);
    uint64 getNowSystemTimeOfSec(void);
    
    // time to string
    // Note: these function deal with from 1970s start time
    char* utcNowTime2String(void);
    char* localNowTime2String(void);
    char* utcSecTime2String(uint64 t);
    char* utcMSecTime2String(uint64 t);
    char* utcMicrosTime2String(uint64 t);

__CExternEnd__
__END__

#endif // __TimeUtil_h__

