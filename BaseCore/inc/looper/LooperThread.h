/*****************************************************************************
* FileName      : LooperThread.h
* Description   : Looper thread definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __LooperThread_h__
#define __LooperThread_h__
#include "MessageLooper.h"
#include "../os/ThreadBase.h"
#include <map>
#include <memory>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <future>
#include <chrono>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__
    
   //------------------------------------------------------------------------------------//
    class LooperThread 
    {
        public:
            LooperThread(const char *name, bool looperInCurrThread = false);

            ~LooperThread(void);

        public:
            Looper& getLooper(void);

            bool quit(void);

            // Note: safe quit will wait all messages that had enqueued message queue been disposed, it so slowly
            bool quitSafely(void);

            std::string& getThreadName(void) { return mThreadName; }

    private:
            void start(void);
            
            void looper(void);

            bool isRunning(void);

        private:
            bool                          mLooperInCurrThread;
            ThreadBase*             mLooperThr;
            std::string                  mThreadName;
            Mutex                       mMutex;
#if (defined(__linux__) || defined(__APPLE__))
            Semaphore               mSem;
            volatile bool             mIsRunning;
#else
            std::promise<bool> mIsRunning;
#endif
            Looper                      mLooper;
            bool                          mLooperExited;

            friend static void threadlooper_entry(void* param);
        };


__END__    

#endif // __LooperThread_h__
