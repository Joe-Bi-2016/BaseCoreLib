/*****************************************************************************
* FileName      : Semaphore.hpp
* Description   : Synchronize semaphore definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __Semaphore_h__
#define __Semaphore_h__
#include "../base/Uncopyable.hpp"
#include "../os/Logger.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdexcept>
#include <limits>
#include <time.h>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <Windows.h>
#elif defined(__APPLE__)
#include <sys/time.h>
#include <dispatch/dispatch.h>
#else
#include <sys/time.h>
#include <semaphore.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (Semaphore):

   //------------------------------------------------------------------------------------//
    class API_EXPORTS Semaphore : private Uncopyable
    {
        public:
            explicit Semaphore(long initCount = 0) : mCount(initCount)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                long max_count = (std::numeric_limits<long>::max)();
                mSemaphore = CreateSemaphore(nullptr, initCount, max_count, nullptr);
                if (mSemaphore == nullptr)
                {
                    LOGE("%s", "fail to create semaphore");
                    exit(EXIT_FAILURE);
                }
#elif (defined(__APPLE__))
                mSemaphore = dispatch_semaphore_create(initCount);
                if(mSemaphore == nullptr)
                 {
                    LOGE("%s", "fail to sem_open");
                    exit(EXIT_FAILURE);
                 }
#else
                int ret = sem_init(&mSemaphore, 0, initCount);
                if(ret != 0)
                 {
                    LOGE("%s", "fail to sem_init");
                    exit(EXIT_FAILURE);
                 }
#endif
            }

            ~Semaphore(void)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mSemaphore != NULL)
                {
                    CloseHandle(mSemaphore);
                    mSemaphore = NULL;
                }
#elif (defined(__APPLE__))
                mSemaphore = nullptr;
#else
                sem_destroy(&mSemaphore);
#endif
            }

            bool wait(void)
            {
                int rc = -1;
                do 
                {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                    DWORD dwWaitResult =WaitForSingleObject(mSemaphore, INFINITE);
                    rc = (dwWaitResult == WAIT_OBJECT_0) ? 0 : -1;
#elif (defined(__APPLE__))
                    rc = dispatch_semaphore_wait(mSemaphore, DISPATCH_TIME_FOREVER);
#else
                    rc = sem_wait(&mSemaphore);
#endif
                } while (rc == -1);

                mCount--;

                return (rc == 0);
            }

            bool wait(long timeoutMs = -1)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                DWORD ret = WaitForSingleObject(mSemaphore, timeoutMs == -1 ? INFINITE : timeoutMs);
                if (ret == WAIT_OBJECT_0)
                    mCount--;
                return ret == WAIT_OBJECT_0 ? true : false;
#elif (defined(__APPLE__))
                int ret = dispatch_semaphore_wait(mSemaphore, timeoutMs == -1 ? DISPATCH_TIME_FOREVER : timeoutMs);
                if (ret == 0)
                    mCount--;
                return ret == 0 ? true : false;
#else
                int ret = 0;
                if (timeoutMs < 0)
                {
                    ret = sem_wait(&mSemaphore);
                    mCount--;
                }
                else if(timeoutMs == 0)
                    return true;
                else
                {
                    struct timeval now;
                    struct timespec out_time;
                    gettimeofday(&now, nullptr);

                    long nsec = now.tv_usec * 1000 + (timeoutMs % 1000) * 1000000;
                    out_time.tv_sec = now.tv_sec + nsec / 1000000000 + timeoutMs / 1000;
                    out_time.tv_nsec = nsec % 1000000000;

                    ret = sem_timedwait(&mSemaphore, &out_time);
                    if(ret != 0 && ret != ETIMEDOUT)
                        LOGE("%s", "sem_timedwait failed: %s", strerror(ret));
                    else
                        mCount--;
                }
               
                return (ret == 0 || ret == ETIMEDOUT);    
#endif
            }

            bool tryWait(void)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))                
                DWORD ret = WaitForSingleObject(mSemaphore, 0);
                if (ret == WAIT_OBJECT_0)
                    mCount--;
                return ret == WAIT_OBJECT_0 ? true : false;
#elif (defined(__APPLE__))
                int ret = dispatch_semaphore_wait(mSemaphore, 0);
                if (ret == 0)
                    mCount--;
                return ret == 0 ? true : false;
#else
                int ret = sem_trywait(&mSemaphore);
                if (ret == 0)
                    mCount--;
                return ret == 0 ? true : false;
#endif                
            }

            bool post(uint32 count = 1)
            {
                if (count <= 0)
                {
                    LOGE("%s", "the parameter of count must larger than 0 !!!");
                    return true;
                }
                
                mCount += count;

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                return ReleaseSemaphore(mSemaphore, count, NULL);
#elif (defined(__APPLE__))
                while (count-- > 0)
                    dispatch_semaphore_signal(mSemaphore);
                return true;
#else
                while(count-- > 0)
                    sem_post(&mSemaphore);
                return true;
#endif
            }

            int getValue(void)
            {
                int value = -1;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                value = mCount;
#elif (defined(__APPLE__))
                value = mCount;
#else
                sem_getvalue(&mSemaphore, &value);
#endif                
                return value;
            }

            int sendMessage(long count = 1)
            {
                return post(count) ? 0 : -1;
            }

            void* getHandler(void) const
            {
                return (void*)(&mSemaphore);
            }

        private: 
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            HANDLE                          mSemaphore;
#elif (defined(__APPLE__))
            dispatch_semaphore_t    mSemaphore;
#else
            sem_t                               mSemaphore;
#endif
            int                                     mCount;
    };

__END__

#endif // __Semaphore_h__
