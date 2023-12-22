/*****************************************************************************
* FileName      : Condition.hpp
* Description   : Synchronize condition definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __Condition_h__
#define __Condition_h__
#include "Mutex.hpp"
#include "Semaphore.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <condition_variable>
#include <time.h>
#else
#include <sys/time.h>
#endif

//---------------------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------------------//
    class API_EXPORTS Condition : private Uncopyable
    {
        public:
            explicit Condition(void)
            : mIsSelfMutex(true)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                mCond = new std::condition_variable();
                if(mCond == nullptr)
                {
                    LOGE("%s", "fail to create condition variable");
                    exit(EXIT_FAILURE);
                }
#else
                int ret =  pthread_cond_init(&mCond, nullptr);
                 if(ret != 0)
                 {
                     LOGE("%s", "fail to pthread_cond_init: %s", stderr(ret));
                    exit(EXIT_FAILURE);
                 }
#endif
                mMutex = new Mutex();
            }

            explicit Condition(Mutex *mutex)
            : mIsSelfMutex(false)
            {
                if (mutex)
                {
                    MutexType type = mutex->getMutexType();
                    if (type != PTHREAD_MUTEX_TIMED_NP && type != PTHREAD_MUTEX_ADAPTIVE_NP)
                    {
                        LOGE("%s", "mutex is not PTHREAD_MUTEX_TIMED_NP or PTHREAD_MUTEX_TIMED_NP type");
                        return;
                    }
                }

                mMutex = mutex;

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                mCond = new std::condition_variable();
                if (mCond == nullptr)
                {
                    LOGE("%s", "fail to create condition variable");
                    exit(EXIT_FAILURE);
                }
#else
                int ret =  pthread_cond_init(&mCond, nullptr);
                 if(ret != 0)
                 {
                     LOGE("%s", "fail to pthread_cond_init: %s", stderr(ret));
                     exit(EXIT_FAILURE);
                 }
#endif
            }

            ~Condition(void)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mCond)
                {
                    delete mCond;
                    mCond = nullptr;
                }
#else
                pthread_cond_destroy(&mCond);
#endif
                if(mIsSelfMutex)
                {
                    delete mMutex;
                    mMutex = nullptr;
                }
            }

            int wait(Mutex* mutex = nullptr, int64 millisecond = -1, bool timeout = false)
            {
                if (mutex == nullptr && mMutex == nullptr)
                {
                    LOGE("%s", "Error!!! parameter error or did not creatte mutex");
                    return -1;
                }
                
                Mutex* pmutex = mutex ? mutex : mMutex;
                MutexType type = pmutex->getMutexType();
                if (type != PTHREAD_MUTEX_TIMED_NP && type != PTHREAD_MUTEX_ADAPTIVE_NP)
                {
                    LOGE("%s", "mutex is not PTHREAD_MUTEX_TIMED_NP or PTHREAD_MUTEX_TIMED_NP type");
                    return -1;
                }

                if (timeout && millisecond < 0)
                {
                    LOGE("%s", "timeout millisecond must larger than 0");
                    return -1;
                }

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (millisecond == 0)
                    return 0;

                std::unique_lock <std::mutex> lck(*(std::mutex*)pmutex->getMutex());

                if (millisecond < 0)
                {
                    mCond->wait(lck);
                    return 0;
                }
                else
                {
                    std::cv_status s = mCond->wait_for(lck, std::chrono::microseconds(millisecond));
                    if (timeout)
                    {
                        if (s == std::cv_status::no_timeout)
                            return 0;
                        return -1;
                    }

                    return s == std::cv_status::no_timeout || s == std::cv_status::timeout;
                }
#else
                if (millisecond == 0)
                    return 0;
                else if(millisecond < 0)
                    return pthread_cond_wait(&mCond, (pthread_mutex_t*)pmutex->getMutex());
                else
                {
                    struct timespec abstime;
#if defined(__APPLE__)
                    timeval now;
                    gettimeofday(&now, nullptr);
                    abstime.tv_sec = now.tv_sec;
                    abstime.tv_nsec = now.tv_usec;
#else
                    (void)clock_gettime(CLOCK_REALTIME, &abstime);
#endif
                    if (timeout)
                    {
                        long nsec = abstime.tv_usec * 1000 + (millisecond % 1000) * 1000000;
                        abstime.tv_sec += nsec / 1000000000 + millisecond / 1000;
                        abstime.tv_nsec = nsec % 1000000000;
                    }
                    else
                    {
                        abstime.tv_sec += millisecond / 1000;
                        abstime.tv_nsec += (millisecond % 1000) * 1000000;
                        while (abstime.tv_nsec >= 1000000000L) {
                            ++abstime.tv_sec;
                            abstime.tv_nsec %= 1000000000L;
                        }
                    }

                    int err = pthread_cond_timedwait(&mCond, (pthread_mutex_t*)pmutex->getMutex(), &abstime);
                    if (err != 0 && err != ETIMEDOUT)
                        LOGE("%s", "pthread_cond_timeout failed: %s", strerror(err));

                    return err;
                }
#endif
            }

            int notifyOne(void)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                mCond->notify_one();
                return 0;
#else
                return pthread_cond_signal(&mCond);    
#endif
            }

            void notify(void)
            {
                notifyOne();
            }

            int notifyAll(void)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                mCond->notify_all();
                return 0;
#else
                return pthread_cond_broadcast(&mCond);
#endif
            }

            void* getMutex(void)
            {
                return mMutex;
            }

        private: 
            bool                               mIsSelfMutex;
            Mutex*                           mMutex;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            std::condition_variable* mCond;
#else
            pthread_cond_t              mCond;
#endif           
    };

__END__

#endif // __Condition_h__
