/*****************************************************************************
* FileName      : ThreadBase.h
* Description   : Thread base definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __ThreadBase_h__
#define __ThreadBase_h__
#include "../base/Macro.h"
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <thread>
#include <Windows.h>
#include <io.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif
#include <future>
#include <functional>
#include <errno.h>
#include <string>

//---------------------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------------------//
    #if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        typedef std::thread NativeThreadHandle;
    #else
        typedef pthread_t NativeThreadHandle;
    #endif

    //------------------------------------------------------------------------------------//
    typedef void(*threadFunc)(void*);
    typedef std::function<void (void*)> threadFuncObj;

    class ThreadBase {
    public:
        explicit ThreadBase(threadFunc func, const std::string& name = "default");
        explicit ThreadBase(threadFuncObj func, const std::string& name = "default");
        ~ThreadBase();

        uint64 getThreadId();

        bool start(void *arg = nullptr, bool syn = false, bool joined = true);

        void join();

        void detach();

        bool joinable() const;

        std::string &getName();

        NativeThreadHandle getNativeThreadHandle();

    private:
        NativeThreadHandle  mHandle; 
        std::string                    mName; 
        bool                            mIsJoined;
        bool                            mIsAttached;
        threadFuncObj            mThreadFunc;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
        std::promise<void*>  mProm;
        std::future<void*>      mFut;
#endif
    };

__END__

#endif // __ThreadBase_h__

