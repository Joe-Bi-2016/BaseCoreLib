/*****************************************************************************
* FileName    : TimerTask.h
* Description : Task timer definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __TimerTask_h__
#define __TimerTask_h__
#include "../base/Macro.h"
#include "../looper/MessageHandler.h"

//---------------------------------------------------------------------------//
__BEGIN__

    class TimerTaskFunc
    {
    public:
        TimerTaskFunc(void) { }
        virtual ~TimerTaskFunc(void) { }
    public:
        virtual void onResponse() = 0;
    };
    
    class TimerTask
    {
    public:
        TimerTask();
        ~TimerTask();
    
        void schedule(TimerTaskFunc *taskfunc, int timeout);
    
    private:
        void sendTimerTaskMsg(int what, int arg1, int arg2 = 0, int timeout = 0, void *data = nullptr, size_t bytes = 0,  const paramDeleter& fnFree = nullptr, bool isRemoved = false);
    
    private:
        static Mutex gMutex;
        threadlocal static Looper  mLoop;
        Handler mHandler;
    };
    
    static void onTimerTaskFuncHandler(const Message& msg, void* context);

__END__

#endif //__TimerTask_h__
