/*****************************************************************************
* FileName    : MessageQueue.h
* Description : Message queue definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __MessageQueue_h__
#define __MessageQueue_h__
#include "Message.h"
#include "../os/AutoMutex.hpp"
#include "../os/Condition.hpp"
#include <string>
#include <list>
#include <memory>
#include <thread>
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
#include <Windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <semaphore.h>
#endif

//---------------------------------------------------------------------------//
__BEGIN__

    //-----------------------------------------------------------------------//
    class API_EXPORTS MsgQueue : private Uncopyable
    {
        friend class MsgLooper;
        friend struct deleter<MsgQueue>;
        
        public:
            void setQueueName(const char* name) { setQueueName(std::string(name)); }

            void setQueueName(const std::string& name);

            const char* getQueueName(void) const { return mName.c_str(); }

            bool enqueueMessage(Message message, uint64 delayDoneTime = 0);

            Message obtain(void);

            bool hasMessage(const Message& message, MsgHandler* handler = nullptr) const;

            bool hasMessage(const runnable& r, MsgHandler* handler = nullptr) const;

            bool hasMessage(int what, MsgHandler* handler = nullptr) const;

            bool hasMessage(const HandlerCallback* callback, MsgHandler* handler = nullptr) const;

            void removeMessage(Message message, MsgHandler* handler = nullptr) noexcept;

            void removeMessage(runnable& r, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int what, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int minWhat, int maxWhat, runnable& r, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int what, int arg1, int arg2, runnable& r, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(HandlerCallback* callback, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int what, HandlerCallback *callback, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int minWhat, int maxWhat, HandlerCallback* callback, MsgHandler* handler = nullptr)  noexcept;

            void removeMessage(int what, int arg1, int arg2, HandlerCallback* callback, MsgHandler* handler = nullptr)  noexcept;

            void removeAllMessages(MsgHandler* handler = nullptr)  noexcept;

            bool isIdle(void) const;

            int getQueueSize(void) const;

            int getMsgPoolSize(void) const;

            void addIdleHandler(const msgQueueIdleHandler& handler);
            void removeIdleHandler(void);

            void dumpQueueList(void) const;

            void dumpQueuePool(void) const;

        private:
            MsgQueue(const std::string& name, int MaxMsgPoolSize = 50);
            ~MsgQueue(void);

            Message next(void); // been call by looper to get message form queue

            void recycleMsg(Message msg)  noexcept;

            void clearMsgPool(void);

            void quit(bool safely = true);

            void setTestOutTimeMillisExit(long t);

        private:
            std::string         mName;
            Message             mMsgQueue;
            int                 mMsgQueueSize;
            mutable Mutex       mLock;
            Condition           mWait;
            Message             mMsgPool;
            int                 mMsgPoolSize;
            int                 mMsgPoolMaxSize;
            bool                mMsgPoolIsFull;
            Mutex               mMsgPoolMutex;
            msgQueueIdleHandler mIdleHandlerFunc;
            bool                mBlocked;
            bool                mQuit;
            bool                mNotEnqueMsg;
            long                mOutTimeTest;
    };


__END__

#endif // __MessageQueue_h__
