/*****************************************************************************
* FileName    : ThreadBase.cpp
* Description : Thread base implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/os/ThreadBase.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../inc/os/Logger.h"
#include <cassert>
#include <sstream>
#if (defined(__linux__) || defined(__ANDROID__))
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__

    //---------------------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (Thread):

    //---------------------------------------------------------------------------------------//
    class threaddata
    {
    private:
        threadFuncObj mFunc;
        std::string mName;
        void *mArg;
        Mutex *mMutex;
    
    public:
        threaddata(threadFuncObj func, std::string &name, void *arg, bool syn)
        {
            this->mFunc = func;
            this->mName = name;
            this->mArg = arg;
    
            if (syn)
                mMutex = new Mutex();
            else
                mMutex = nullptr;
        }
    
        ~threaddata()
        {
            if (mMutex)
            {
                delete mMutex; 
                mMutex = nullptr;
            }
        }
    
        void runThreadFunc()
        {
            LOGI("%s\n", "------------- RUNNING User-thread function -------------");
            this->unlock();
            this->mFunc(this->mArg);
            LOGI("%s\n", "--------------- User-thread function  END  ----------------");
        }
    
        void lock()
        {
            if (mMutex)
                mMutex->lock();
        }
    
        void unlock()
        {
            if (mMutex)
                mMutex->unlock();
        }
    
        std::string &getName()
        {
            return mName;
        }
    };
    
    //---------------------------------------------------------------------------------------//
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    static unsigned int startThread(std::future<void*>& arg)
    {
        threaddata* data = (threaddata*)arg.get();
        {
            const char* name = data->getName().c_str();
            char threadName[64] = { 0 };
            snprintf(threadName, 64, "thread-%s", name ? name : "default");
            LOGD("threadName = %s", threadName);
        }

        data->runThreadFunc();
        delete data;
        data = nullptr;

        return 0;
    }
#else
    static void* startThread(void *arg)
    {
        threaddata* data = static_cast<threaddata*>(arg);
        {
            const char *name = data->getName().c_str();
            char threadName[64] = {0};
            snprintf(threadName, 64, "thread-%s", name ? name : "default");
            prctl(PR_SET_NAME, threadName);
            LOGD("threadName = %s", threadName);
        }
    
        data->runThreadFunc();
        delete data;
        data = nullptr;

        return nullptr;
    }
#endif

    //---------------------------------------------------------------------------------------//
    ThreadBase::ThreadBase(threadFunc func, const std::string &name /*= "default"*/)
        : mName(name)
#if (defined(__linux__) || defined(__APPLE__) || defined(TARGET_OS_IPHONE) || defined(__ANDROID__))
        , mHandle(0)
#endif
        , mIsJoined(false)
        , mIsAttached(true)
        , mThreadFunc(func)
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        , mFut(mProm.get_future())
#endif
    {
    }

    //---------------------------------------------------------------------------------------//
    ThreadBase::ThreadBase(threadFuncObj func, const std::string& name /*= "default"*/)
        : mName(name)
#if (defined(__linux__) || defined(__APPLE__) || defined(TARGET_OS_IPHONE) || defined(__ANDROID__))
        , mHandle(0)
#endif
        , mIsJoined(false)
        , mIsAttached(true)
        , mThreadFunc(std::move(func))
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        , mFut(mProm.get_future())
#endif
    {
    }
    
    //---------------------------------------------------------------------------------------//
    ThreadBase::~ThreadBase()
    {
        mName = "";
        mIsJoined = false;
        mIsAttached = true;
        mThreadFunc = nullptr;
    }
    
    //---------------------------------------------------------------------------------------//
    bool ThreadBase::joinable() const
    {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        return mHandle.joinable();
#else
        return mIsJoined && !mIsAttached;
#endif
    }
    
    //---------------------------------------------------------------------------------------//
    std::string & ThreadBase::getName()
    {
        return mName;
    }
    
    //---------------------------------------------------------------------------------------//
    NativeThreadHandle ThreadBase::getNativeThreadHandle()
    {
        return std::move(mHandle);
    }
    
    //---------------------------------------------------------------------------------------//
    #if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
    bool ThreadBase::start(void *arg /*= nullptr*/, bool syn /*= false*/, bool enJoined /*= true*/)
    {
        threaddata* data = new threaddata(mThreadFunc, mName, arg, false);
        if (!syn) 
            mProm.set_value(data);
        mHandle = std::thread(startThread, std::ref(mFut));

        mIsJoined = enJoined;
        mIsAttached = mIsJoined ? false : true;

        if (syn)
            mProm.set_value(data);

        return true;
    }
    
    //---------------------------------------------------------------------------------------//
    void ThreadBase::join()
    {
        if (joinable())
        {
            mHandle.join();
            mIsJoined = false;
        }
    }
    
    //---------------------------------------------------------------------------------------//
    void ThreadBase::detach()
    {
        if (!mIsJoined && mIsAttached)
        {
            mHandle.detach();
            mIsAttached = false;
        }
    }
    
    //---------------------------------------------------------------------------------------//
    uint64 ThreadBase::getThreadId()
    {
        std::stringstream ss;
        ss << mHandle.get_id();
        return std::stoi(ss.str());
    }
    
    //---------------------------------------------------------------------------------------//
    #elif (defined(__linux__) || defined(__APPLE__) || defined(TARGET_OS_IPHONE) || defined(__ANDROID__))
    bool ThreadBase::start(void *arg /*= nullptr*/, bool syn /*= false*/, bool enJoined /*= true*/)
    {
        bool ret = true;
        threaddata* data = new threaddata(mThreadFunc, mName, arg, syn);
        if(data == nullptr)
        {
            LOGE("%s", "create thread parmeter error");
            return false;
        }
    
        if (pthread_create(&mHandle, nullptr, startThread, data) != 0)
        {
            mHandle = 0;
            ret = false;
            delete data;
            LOGE("%s", "start thread name = %s error", mName.c_str());
        }
        else
        {
            data->lock();
        }
    
        mIsJoined = enJoined;
        mIsAttached = mIsJoined ? false : true;
    
        return ret;
    }
    
    //---------------------------------------------------------------------------------------//
    void ThreadBase::join()
    {
        if (joinable())
        {
            pthread_join(mHandle, nullptr);
            mHandle = 0;
            mIsJoined = false;
        }
    }
    
    //---------------------------------------------------------------------------------------//
    void ThreadBase::detach()
    {
        if (!mIsJoined && mIsAttached && mHandle)
        {
            pthread_detach(mHandle);
            mHandle = 0;
            mIsNotAttach = false;
        }
    }
    
    //---------------------------------------------------------------------------------------//
    uint64 ThreadBase::getThreadId()
    {
        return (uint64)mHandle;
    }
    
    #endif

__END__
