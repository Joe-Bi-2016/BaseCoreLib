/*****************************************************************************
* FileName    : CondSyncLock.hpp
* Description : Multithread synchronize using mutex and condition lock definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __CondSyncLock_h__
#define __CondSyncLock_h__
#include "Condition.hpp"
#include <functional>
#include <condition_variable>

//---------------------------------------------------------------------------//
__BEGIN__

    //-----------------------------------------------------------------------//
    template<typename ...Args>
    class API_EXPORTS syncondLock : private Uncopyable
    {
        public:
            explicit syncondLock(void)
            : mEnable(true)
            , mSelfCondVal(true)
            , mMyCond(new Condition())
            {  }

            explicit syncondLock(Condition* cond)
            : mEnable(true)
            , mSelfCondVal(false)
            , mMyCond(cond)
            {  }

            void runTask(const std::function<void(Args... args)>& task)
            {
                ((Mutex*)(mMyCond->getMutex()))->lock();

                while (!mEnable)
                    mMyCond->wait();

                task();

                mEnable = false;

                ((Mutex*)(mMyCond->getMutex()))->unlock();
            }

            void reset(void)
            {
                ((Mutex*)(mMyCond->getMutex()))->lock();
                mEnable = true;
                mMyCond->notifyAll();
                ((Mutex*)(mMyCond->getMutex()))->unlock();
            }

            ~syncondLock()
            {
                reset();
                if (mSelfCondVal)
                {
                    delete mMyCond;
                    mMyCond = nullptr;
                }
            }

        private:
            Condition*  mMyCond;
            bool        mEnable;
            bool        mSelfCondVal;
    };

__END__

#endif // __CondSyncLock_h__
