/*****************************************************************************
* FileName    : MessageLooper.cpp
* Description : Message looper implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/looper/MessageLooper.h"
#include "../../inc/looper/MessageQueue.h"
#include "../../inc/looper/MessageHandler.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../inc/os/Logger.h"
#include <algorithm>
#include <iostream>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <windows.h>
#include <io.h>
#include <process.h>
#include <processthreadsapi.h>
#elif (defined(__linux__) || defined(__ANDROID__))
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sched.h>
#include <pthread.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (MessageLooper):

   //------------------------------------------------------------------------------------//
    static uint64 getCurrentThreadId(void)
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        return GetCurrentThreadId();
#elif defined(__linux__)
        return syscall(SYS_gettid);
#elif (defined(__APPLE__) && defined(__MACH__))
    #if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12
        uint64_t tid64;
        pthread_threadid_np(nullptr, &tid64);
        return tid64;
    #else
        return syscall(__NR_gettid);
    #endif
#endif
    }

   //------------------------------------------------------------------------------------//
    thread_local Looper MsgLooper::mThreadLocal(nullptr);
    thread_local Mutex MsgLooper::mMutex(PTHREAD_MUTEX_RECURSIVE_NP);

   //------------------------------------------------------------------------------------//
    MsgLooper::MsgLooper(const char* msgQueueName, int msgQueueMaxCnt, uint64 tid)
    : mQueue(0)
    , mThreadId(0)
    , mExit(false)
    , mPromoteThrLevel(false)
    {
        mQueue = Queue(new MsgQueue(msgQueueName, msgQueueMaxCnt), deleter<MsgQueue>());
        mThreadId = tid;
        LOGD("Message queue name = %s, ThreadId = %llu", msgQueueName, mThreadId);
    }

   //------------------------------------------------------------------------------------//
    MsgLooper::~MsgLooper(void)
    {
        quit();
        mThreadId = 0;
        mExit = true;
        mPromoteThrLevel = false;
        LOGD("%s", "Looper been destroyed!");
    }

   //------------------------------------------------------------------------------------//
    Looper MsgLooper::prepare(int msgQueueMaxSize /* = 50 */)
    {
        AutoMutex lock(&mMutex);

        if (mThreadLocal == nullptr) 
        {
            uint64 tid = getCurrentThreadId();
            char name[64] = { 0 };
            SNPRINTF(name, 64, "%s%llu%s", "Thread_", tid, "_MsgQueue");
            mThreadLocal = Looper(new MsgLooper(name, msgQueueMaxSize, tid), deleter<MsgLooper>());
        }

        return mThreadLocal;
    }

   //------------------------------------------------------------------------------------//
    Looper MsgLooper::myLooper(void)
    {
        AutoMutex lock(&mMutex);

        if(mThreadLocal == nullptr)
        {
            LOGE("%s", "Error: current thread had not create MsgLooper, you should call MsgLooper.prepare() first");
            return Looper(nullptr);
        }

        return mThreadLocal;
    }

   //------------------------------------------------------------------------------------//
    void MsgLooper::loop(void)
    {
        AutoMutex lock(&mMutex);

        Looper loop = myLooper();
        if(loop.get() == nullptr)
        {
            LOGW("%s", "No looper; MsgLooper.prepare() wasn't called on this thread.");
            return;
        }

        Queue queue = loop->getMsgQueue(); 
        for(;;)
        {
            if (mExit && queue.get() == nullptr)
            {
                LOGW("%s", "Warning: Message looper had exited and Message queue is empty.");
                return;
            }

            // safely exit, the messge in queue does not been consumed, improve current thread level for 
            // accelerating execution
            if (mExit && mPromoteThrLevel)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                int policy = GetThreadPriority(GetCurrentThread());
                policy += THREAD_PRIORITY_ABOVE_NORMAL;
                SetThreadPriority(GetCurrentThread(), policy);
                mPromoteThrLevel = false;
#else
                int policy;
                sched_param sch;
                pthread_getschedparam(pthread_self(), &policy, &sch);
                sch.sched_priority += 1;
                pthread_setschedparam(thread, policy, &sch);
#endif
            }

            if(queue.get() == nullptr)
            {
                LOGE("%s", "Error: Message queue had been destroyed.");
                return;
            }

            Message msg = queue->next();
            if(msg.get())
            {
                assert(msg->mTarget);
                msg->mTarget->dispatchMessage(msg);
                queue->recycleMsg(std::move(msg));
            }
            else
            {
                LOGI("%s", "Message is null, exit looper");
                return;
            }
        }
    }

   //------------------------------------------------------------------------------------//
    void MsgLooper::quit(bool safely /*= false */)
    {
        AutoMutex lock(&mMutex);

        if(mExit)
            return;

        if(mQueue)
            mQueue->quit(safely);
        
        if (safely)
            mPromoteThrLevel = true;

        mExit = true;    
    }


__END__