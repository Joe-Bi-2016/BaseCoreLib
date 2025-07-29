/*****************************************************************************
* FileName    : LooperThread.cpp
* Description : Looper thread implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/looper/LooperThread.h"
#include "../../inc/looper/MessageHandler.h"
#include "../../inc/os/Logger.h"
#include <iostream>

//---------------------------------------------------------------------------//
__BEGIN__
    
   //------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (LooperThread):
    
   //------------------------------------------------------------------------//
    void threadlooper_entry(void *param)
     {
        LooperThread *_this = static_cast<LooperThread*>(param);
        LOGD("threadlooper_entry:running handler thread name = %s", _this->getThreadName().c_str());
        _this->looper();
    }

   //------------------------------------------------------------------------//
    LooperThread::LooperThread(const char* name, int msgPoolSize/* = 50*/,  bool looperInCurrThread /*= false*/)
    : mLooperThr(nullptr) 
    , mThreadName(name)
    , mLooperInCurrThread(looperInCurrThread)
#if (defined(__linux__) || defined(__APPLE__))
    , mIsRunning(false)
#endif
    , mMsgPoolSize(msgPoolSize)
    , mLooperExited(false)
    {
        mLooper = mLooperInCurrThread ? MsgLooper::prepare(mMsgPoolSize) : nullptr;
    }

   //------------------------------------------------------------------------//
    LooperThread::~LooperThread() 
    {
        AutoMutex lock(&mMutex);

        mLooperExited = true;
        if (!mLooperInCurrThread)
        {
            if (mLooperThr)
            {
                mLooper->quit();
                mLooperThr->join();
                delete mLooperThr;
                mLooperThr = nullptr;
            }
        }
        else
            mLooper->quit();
    }

   //------------------------------------------------------------------------//
    Looper& LooperThread::getLooper(void)
    {
        if (!mLooperInCurrThread)
        {
            this->start();
#if (defined(__linux__) || defined(__APPLE__))
            while (!isRunning())
            {
                mSem.wait(100);
            }
#else
            AutoMutex lock(&mMutex);
            if (mLooper == nullptr)
            {
                std::future<bool> fu = mIsRunning.get_future();
                LOGD("%s Looper thread is been start running....", fu.get() ? "Yes" : "No");
            }
#endif
        }

        return mLooper;
    }

   //------------------------------------------------------------------------//
    bool LooperThread::quit(void)
    {
        AutoMutex lock(&mMutex);
        if (mLooperExited)
            return true;

        if(mLooper)
        {
            mLooper->quit(false);
            mLooperExited = true;
            return true;
        }

        return false;
    }

   //------------------------------------------------------------------------//
    bool LooperThread::quitSafely(void)
    {
        AutoMutex lock(&mMutex);
        if (mLooperExited)
            return true;;

        if(mLooper)
        {
            mLooper->quit(true);
            mLooperExited = true;
            return true;
        }

        return false;
    }

    //------------------------------------------------------------------------//
    void LooperThread::start(void)
    {
        if (!mLooperInCurrThread)
        {
            AutoMutex lock(&mMutex);
            if (!mLooperThr)
            {
                mLooperThr = new ThreadBase(threadlooper_entry, mThreadName);
                mLooperThr->start(this, false);
            }
        }
    }

   //------------------------------------------------------------------------//
    void LooperThread::looper(void)
    {
        if(mLooper.get() == nullptr)
        {
            mLooper = MsgLooper::prepare(mMsgPoolSize);
#if (defined(__linux__) || defined(__APPLE__))
            mIsRunning = true;
            mSem.post();
#else
            mIsRunning.set_value(true);
#endif
            LOGD("%s", "entry looper thread......");
        }
        else
            LOGD("%s", "entry looper circulating......");

        mLooper->loop();
    }

   //------------------------------------------------------------------------//
    bool LooperThread::isRunning(void)
    {
        if (!mLooperInCurrThread)
        {
#if (defined(__linux__) || defined(__APPLE__))
            AutoMutex lock(&mMutex);
            return mIsRunning && (mLooper.get());
#endif
        }
        return true;
    }

__END__