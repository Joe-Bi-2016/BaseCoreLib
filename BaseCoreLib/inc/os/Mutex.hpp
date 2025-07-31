/*****************************************************************************
* FileName    : Mutex.hpp
* Description : Synchronize mutex definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __Mutex_h__
#define __Mutex_h__
#include "../base/Uncopyable.hpp"
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <mutex>
#include <Windows.h>
#else
#include <pthread.h>
#endif

//---------------------------------------------------------------------------//
__BEGIN__

   //------------------------------------------------------------------------//
    typedef enum
    {
        PTHREAD_MUTEX_TIMED_NP      = 0, // defalut
        PTHREAD_MUTEX_ADAPTIVE_NP   = 1, // normal
        PTHREAD_MUTEX_RECURSIVE_NP  = 2, // recursive
        PTHREAD_MUTEX_ERRORCHECK_NP = 3, // error check
    }MutexType;

    class API_EXPORTS Mutex : private Uncopyable
    {
        public:
            explicit Mutex(MutexType type = PTHREAD_MUTEX_TIMED_NP) 
             : mMutexType(type)
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mMutexType == PTHREAD_MUTEX_TIMED_NP)
                    mMutex = new std::mutex();
                else if (mMutexType == PTHREAD_MUTEX_ADAPTIVE_NP)
                    mMutex = new std::mutex(); //new std::timed_mutex();
                else if (mMutexType == PTHREAD_MUTEX_RECURSIVE_NP)
                    mMutex = new std::recursive_mutex();
                else
                    mMutex = new std::recursive_timed_mutex();
#else
                int initType;
                if (type == PTHREAD_MUTEX_TIMED_NP)
                    initType = PTHREAD_MUTEX_DEFAULT;
                else if (type == PTHREAD_MUTEX_ADAPTIVE_NP)
                    initType = PTHREAD_MUTEX_NORMAL;
                else if (type == PTHREAD_MUTEX_RECURSIVE_NP)
                    initType = PTHREAD_MUTEX_RECURSIVE;
                else
                    initType = PTHREAD_MUTEX_ERRORCHECK;

                pthread_mutexattr_t attr = { 0 };
                pthread_mutexattr_init(&attr);
                pthread_mutexattr_settype(&attr, initType);
                pthread_mutex_init(&mMutex, &attr);
                pthread_mutexattr_destroy(&attr);
#endif
            }

            ~Mutex(void)
            {
                if (!mMutex) return;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                delete mMutex; mMutex = nullptr;
#else
                pthread_mutex_destroy(&mMutex);
#endif
            }

            void lock(void)
            {
                if (!mMutex) return;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mMutexType == PTHREAD_MUTEX_TIMED_NP)
                    ((std::mutex*)mMutex)->lock();
                else if (mMutexType == PTHREAD_MUTEX_ADAPTIVE_NP)
                    ((std::mutex*)mMutex)->lock();
                else if (mMutexType == PTHREAD_MUTEX_RECURSIVE_NP)
                    ((std::recursive_mutex*)mMutex)->lock();
                else
                    ((std::recursive_timed_mutex*)mMutex)->lock();
#else
                pthread_mutex_lock(&mMutex);
#endif
            }

            int trylock(void)
            {
                if (!mMutex) return -1;
                bool ret = false;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mMutexType == PTHREAD_MUTEX_TIMED_NP)
                    ret = ((std::mutex*)mMutex)->try_lock();
                else if (mMutexType == PTHREAD_MUTEX_ADAPTIVE_NP)
                    ret = ((std::mutex*)mMutex)->try_lock();
                else if (mMutexType == PTHREAD_MUTEX_RECURSIVE_NP)
                    ret = ((std::recursive_mutex*)mMutex)->try_lock();
                else
                    ret = ((std::recursive_timed_mutex*)mMutex)->try_lock();
#else
                pthread_mutex_trylock(&mMutex) == 0 ? ret = true : false;
#endif
                return ret ? 0 : -1;
            }

            void unlock(void)
            {
                if (!mMutex) return;
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                if (mMutexType == PTHREAD_MUTEX_TIMED_NP)
                    ((std::mutex*)mMutex)->unlock();
                else if (mMutexType == PTHREAD_MUTEX_ADAPTIVE_NP)
                    ((std::mutex*)mMutex)->unlock();
                else if (mMutexType == PTHREAD_MUTEX_RECURSIVE_NP)
                    ((std::recursive_mutex*)mMutex)->unlock();
                else
                    ((std::recursive_timed_mutex*)mMutex)->unlock();
#else
                pthread_mutex_unlock(&mMutex);
#endif
            };

            void* getMutex(void) const
            {
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                return mMutex;
#else
                return (void*)&mMutex;
#endif
            }

            MutexType getMutexType(void)
            {
                return mMutexType;
            }

        private: 
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            void*           mMutex;
#else
            pthread_mutex_t mMutex;
#endif           
            MutexType       mMutexType;
    };

    template <class MyMutex>
    class LockGuard
    {
    public:
        explicit LockGuard(MyMutex& mutex) : mMutex(mutex)
        {
            mMutex.lock();
        }

        ~LockGuard()
        {
            mMutex.unlock();
        }

    private:
        MyMutex& mMutex;
    };

    template <class MyMutex>
    class TryLockGuard: private Uncopyable
    {
    public:
        explicit TryLockGuard(MyMutex& mutex) : mMutex(mutex)
        {
            mIsLocked = mMutex.tryLock();
        }

        ~TryLockGuard()
        {
            if (mIsLocked)
            {
                mMutex.unlock();
            }
        }

        bool isLocked()
        {
            return mIsLocked;
        }

    private:
        bool mIsLocked;
        MyMutex& mMutex;
    };
    
__END__

#endif // __Mutex_h__
