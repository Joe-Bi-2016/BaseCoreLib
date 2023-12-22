/*****************************************************************************
* FileName      : MessageLooper.h
* Description   : Message looper definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __MessageLooper_h__
#define __MessageLooper_h__
#include "MessageQueue.h"
#include "../os/Mutex.hpp"
#include <memory>
#include <thread>

//---------------------------------------------------------------------------------------//
__BEGIN__
    
    //------------------------------------------------------------------------------------//
    class API_EXPORTS MsgLooper : private Uncopyable
    {
        friend struct deleter<MsgLooper>;

        public:            

            static Looper prepare(int msgQueueMaxSize = 50);

            static Looper myLooper(void);

            static Queue& myQueue(void) { return myLooper()->getMsgQueue(); }

            void loop(void);

            Queue& getMsgQueue(void) { AutoMutex lock(&mMutex); return mQueue; }

            void quit(bool safely = false);

            bool hadExit(void) { AutoMutex lock(&mMutex); return mExit; }

            uint64 getThredId(void) const { AutoMutex lock(&mMutex); return mThreadId; }

            void setTestWaitTime(long outTimeMillisExit) { getMsgQueue()->setTestOutTimeMillisExit(outTimeMillisExit); }

        private:
            MsgLooper(const char* msgQueueName, int msgQueueMaxCnt, uint64 tid);
            ~MsgLooper(void);
            
        private:
            thread_local static Looper  mThreadLocal;
            Queue                                 mQueue;
            uint64                                  mThreadId;
            thread_local static Mutex    mMutex;
             bool                                    mExit;
             bool                                    mPromoteThrLevel;
    };

__END__

#endif // __MessageLooper_h__
