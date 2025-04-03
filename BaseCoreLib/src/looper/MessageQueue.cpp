/*****************************************************************************
* FileName    : MessageQueue.cpp
* Description : Message queue implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/looper/MessageQueue.h"
#include "../../inc/looper/MessageHandler.h"
#include "../../inc/looper/MessageLooper.h"
#include "../../inc/os/Logger.h"
#include "../../inc/base/TimeUtil.h"
#include <algorithm>

//---------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (MessgeQueue):

    //------------------------------------------------------------------------//
    #define RECYCLEMIDDLE(p, prev, handler)  \
        if (handler == nullptr || (handler && p->mTarget == handler))\
        {\
              Message msg(nullptr); \
              if (prev == p) { \
                    msg = std::move(mMsgQueue); \
                    mMsgQueue = std::move(p->mNext);\
              } \
              else {\
                    msg = std::move(prev->mNext); \
                    prev->mNext = std::move(p->mNext); \
              }\
              recycleMsg(std::move(msg)); \
              mMsgQueueSize--; \
              break;\
         }\
        else {\
              prev = p;\
              p = p->mNext.get();\
         }\

   //------------------------------------------------------------------------//
    MsgQueue::MsgQueue(const std::string& name, int MaxMsgPoolSize /* = 50 */)
    : mName(name)
    , mMsgQueue(nullptr)
    , mMsgQueueSize(0)
    , mLock()
    , mWait(nullptr)
    , mMsgPool(nullptr)
    , mMsgPoolSize(0)
    , mMsgPoolMaxSize(MaxMsgPoolSize)
    , mMsgPoolIsFull(false)
    , mMsgPoolMutex()
    , mIdleHandlerFunc(nullptr)
    , mBlocked(true)
    , mQuit(false)
    , mNotEnqueMsg(false)
    , mOutTimeTest(0)
    {
    }

   //------------------------------------------------------------------------//
    MsgQueue::~MsgQueue(void)
    {
        {
            AutoMutex critical(&mLock);

            Message pre;
            while (mMsgQueue)
            {
                pre = std::move(mMsgQueue);
                mMsgQueue = std::move(pre->mNext);
                pre.reset();
            }

            mName = "";
            mMsgQueueSize = 0;
            mIdleHandlerFunc = nullptr;
            mBlocked = false;
            mQuit = true;
            mNotEnqueMsg = false;
        }
        {
            clearMsgPool();
            mMsgPoolMaxSize = 0;
        }

        LOGD("%s", "Message queue been destroyed!");   
    }

   //------------------------------------------------------------------------//
    void MsgQueue::setQueueName(const std::string& name)
    {
       AutoMutex critical(&mLock);     
        mName = name;
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::enqueueMessage(Message message, uint64 delayDoneTime /* = 0 */)
    {
        if(message->mTarget == nullptr)
        {
            LOGW("%s", "message handler is null. COULDN'T BEEN ADDED TO MESSAGE QUEUE!");
            return false;
        }

        if(message->isInUse())
        {
            LOGW("%s", "message is been using");
            return false;
        }   

        AutoMutex critical(&mLock); 
        
        if(mQuit || mNotEnqueMsg)
        {
            LOGE("%s", "Error: Message queue had exited.");
            recycleMsg(std::move(message));
            return false;
        }

        message->makeInUse();
        message->mWhen = delayDoneTime;
       
        if(mMsgQueue.get() == nullptr || delayDoneTime == 0 || delayDoneTime < mMsgQueue->mWhen)
        {
            message->mNext = std::move(mMsgQueue);
            mMsgQueue = std::move(message);
        }
        else
        {
            Msg* h = mMsgQueue.get();
            Msg* prev;
            for(;;)
            {
                prev = h;
                h = h->mNext.get();
                if(h == nullptr || delayDoneTime < h->mWhen)
                    break;
            }

            message->mNext = std::move(prev->mNext);
            prev->mNext = std::move(message);
        } 

        mMsgQueueSize++;   
        mBlocked = false; 
        mWait.notifyAll();   
        
        return true;
    }

   //------------------------------------------------------------------------//
    Message MsgQueue::obtain(void)
    {
        AutoMutex critical(&mMsgPoolMutex);
        Message ret(nullptr);

        if (mMsgPoolSize >0 && mMsgPoolSize <= mMsgPoolMaxSize && mMsgPool && mMsgPoolIsFull)
        {
            ret = std::move(mMsgPool);
            mMsgPool = std::move(ret->mNext);
            mMsgPoolSize--;
            if (mMsgPoolSize == 0)
                mMsgPoolIsFull = false;
        }

        return ret;  
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::hasMessage(const Message& message, MsgHandler* handler /* = nullptr */) const
    {
        bool ret = false;
        if (message->mTarget == nullptr)
        {
            LOGE("%s", "parameter of message's handler is null");
            return ret;
        }
        
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGW("%s", "Message queue is empty");
            return ret;
        }

        Msg* h = mMsgQueue.get();
        for(;;)
        {
            if(h == nullptr)
                break;

            if(h == message.get())
            {
                ret = handler ? (h->mTarget == handler ? true : false) : true;
                if (ret)
                    break;
            }
            
            h = h->mNext.get();
        }

        return ret;
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::hasMessage(const runnable& r, MsgHandler* handler /* = nullptr */) const
    {
        bool ret = false;
        if (r == nullptr)
        {
            LOGE("%s", "parameter of runnable is null");
            return ret;
        }
        
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return ret;
        }

        Msg* h = mMsgQueue.get();
        for(;;)
        {
            if(h == nullptr)
                break;

            if (h->mCallback == r)
            {
                ret = handler ? (h->mTarget == handler ? true : false) : true;
                if (ret)
                    break;
            }

            h = h->mNext.get();
        }

        return ret;
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::hasMessage(int what, MsgHandler* handler /* = nullptr */) const
    {
        bool ret = false;

        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return ret;
        }

        Msg* h = mMsgQueue.get();
        for(;;)
        {
            if(h == nullptr)
                break;

            if (h->mWhat == what)
            {
                ret = handler ? (h->mTarget == handler ? true : false) : true;
                if (ret)
                    break;
            }

            h = h->mNext.get();
        }

        return ret;
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::hasMessage(const HandlerCallback* callback, MsgHandler* handler /* = nullptr */) const
    {
        bool ret = false;

        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return ret;
        }

        Msg* h = mMsgQueue.get();
        for(;;)
        {
            if(h == nullptr)
                break;

            if (h->mHandleCallback == callback)
            {
                ret = handler ? (h->mTarget == handler ? true : false) : true;
                if (ret)
                    break;
            }

            h = h->mNext.get();
        }

        return ret;
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(Message message, MsgHandler* handler /* = nullptr */) noexcept
    {
        if(message.get() == nullptr)
        {
            LOGE("%s", "parameter of message is null");
            return;
        }
        
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty£¬ the message will been recycled into message pool.");
            recycleMsg(std::move(message));
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if(h == nullptr)
                break;

            if (h == message.get())
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(runnable& r, MsgHandler* handler /* = nullptr */) noexcept
    {
        if(r == nullptr)
        {
            LOGE("%s", "parameter of runnabel is null");
            return;
        }

        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;

            if (h->mCallback == r)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int what, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            if (h->mWhat == what)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int minWhat, int maxWhat, runnable& r, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            bool b = h->mWhat >= minWhat && h->mWhat <= maxWhat && h->mCallback == r; 
            if (b)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int what, int arg1, int arg2, runnable& r, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            bool b = h->mWhat == what && h->mArg1 == arg1 && h->mArg2 == arg2 && h->mCallback == r; 
            if (b)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(HandlerCallback* callback, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            if (h->mHandleCallback == callback)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int what, HandlerCallback *callback, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            bool b = h->mWhat == what && h->mHandleCallback == callback; 
            if (b)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int minWhat, int maxWhat, HandlerCallback* callback, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            bool b = (h->mWhat >= minWhat && h->mWhat <= maxWhat) && (h->mHandleCallback == callback); 
            if (b)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeMessage(int what, int arg1, int arg2, HandlerCallback* callback, MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
            LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if (h == nullptr)
                break;
            
            bool b = h->mWhat == what && h->mArg1 == arg1 && h->mArg2 == arg2 && h->mHandleCallback == callback; 
            if (b)
            {
                RECYCLEMIDDLE(h, prev, handler);
            }
            else
            {
                prev = h;
                h = h->mNext.get();
            }
        }
    }

   //------------------------------------------------------------------------//
    void MsgQueue::removeAllMessages(MsgHandler* handler /* = nullptr */)  noexcept
    {
        AutoMutex critical(&mLock);

        if(mMsgQueue.get() == nullptr)
        {
          LOGE("%s", "Message queue is empty");
            return;
        }

        Msg* h = mMsgQueue.get();
        Msg* prev = h;
        for(;;)
        {
            if(handler)
            {
                if(h == nullptr)
                    break;

                if(h->mTarget == handler)
                {
                    Message msg(nullptr);

                    if (h == mMsgQueue.get()) // head
                    {
                        msg = std::move(mMsgQueue);
                        mMsgQueue = std::move(msg->mNext); 
                    }
                    else
                    {
                        msg = std::move(prev->mNext);
                        prev->mNext = std::move(h->mNext);
                    }

                    recycleMsg(std::move(msg));
                    mMsgQueueSize--;

                    h = prev ? prev->mNext.get() : mMsgQueue.get();
                }
                else
                {
                    prev = h;
                    h = h->mNext.get();
                }
            }
            else
            {
                if (mMsgQueue.get() == nullptr)
                    break;

                Message msg = std::move(mMsgQueue);
                mMsgQueue = std::move(mMsgQueue->mNext);
                recycleMsg(std::move(msg));
                mMsgQueueSize--;
            }
        }
    }

   //------------------------------------------------------------------------//
    bool MsgQueue::isIdle(void) const
    {
        bool ret = false;    
        AutoMutex critical(&mLock);
        uint64 now = getNowTimeOfMs();
        ret = (mMsgQueue.get() == nullptr || (now < mMsgQueue->mWhen));

        return ret;
    }    

   //------------------------------------------------------------------------//
    int MsgQueue::getQueueSize(void) const
    {
        AutoMutex critical(&mLock);
        return mMsgQueueSize;
    }    

   //------------------------------------------------------------------------//
    int MsgQueue::getMsgPoolSize(void) const
    {
        AutoMutex critical((Mutex* const)&mMsgPoolMutex);
        return mMsgPoolSize;
    }  

   //------------------------------------------------------------------------//    
    Message MsgQueue::next(void)
    {
        Message ret(nullptr);
        long nextPollMsgTimeoutMillis = -1;

        for(;;)
        {
            mLock.lock();

            if (mQuit)
            {
                LOGW("%s", "Warning: message queue exited, that is could not been using. RETURN!!!");
                mLock.unlock();
                return Message(nullptr);
            }

            // safely exit, no message in queue, so did not wait
            if (mNotEnqueMsg)
                nextPollMsgTimeoutMillis = 0;

            // because c++11 condition_variable use std::unique_lock <std::mutex>, so must unlock mutex and lock mutex after
            // calling wait-function
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
            mLock.unlock();
#endif

            if(nextPollMsgTimeoutMillis == -1)
            {
                while (mBlocked)
					mWait.wait(&mLock, nextPollMsgTimeoutMillis);
					
#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
				mLock.lock();
#endif
            }
            else
            {
                mWait.wait(&mLock, nextPollMsgTimeoutMillis);

#if (defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__))
                mLock.lock();
#endif
                // test outtime thread exit
                if(mOutTimeTest > 0 && nextPollMsgTimeoutMillis == mOutTimeTest)
                {
                    LOGI("%s", "OutTime! exit queue!");
                    mLock.unlock();
                    return Message(nullptr);
                }
            }

            uint64 now = getNowTimeOfNs() / PER_SEC_USEC;
            Msg* h = mMsgQueue.get();
            Msg* prev(nullptr);

            if(h)
            {
                if(h->mWhen <= now)
                {
                    if (prev == nullptr)
                    {
                        ret = std::move(mMsgQueue);
                        mMsgQueue = std::move(ret->mNext);
                    }
                    else
                    {
                        ret = std::move(prev->mNext);
                        prev->mNext = std::move(ret->mNext);
                    }
                    mMsgQueueSize--;

                    mLock.unlock();
                    break;
                }
                else
                {
                    nextPollMsgTimeoutMillis = long(mMsgQueue->mWhen - now);
                }
            }
            else
            {
                if (mQuit || mNotEnqueMsg)
                {
                    mLock.unlock();
                    return Message(nullptr);
                }

                // No message in queue, so must block
                if(mOutTimeTest > 0)
                    nextPollMsgTimeoutMillis = mOutTimeTest;
                else
                {
                    nextPollMsgTimeoutMillis = -1;
                    mBlocked = true;
                }    
            }
            
            mLock.unlock();
        }

        return ret;
    }    

   //------------------------------------------------------------------------//
    void MsgQueue::recycleMsg(Message msg)  noexcept
    {
        msg->recycleUnchecked();
        AutoMutex critical((Mutex* const)&mMsgPoolMutex);
        
        if (mQuit || mMsgPoolSize == mMsgPoolMaxSize)
        {
            msg.reset();
            return;
        }

        msg->mNext = std::move(mMsgPool);
        mMsgPool = std::move(msg);

        mMsgPoolSize++;

        if (mMsgPoolSize == mMsgPoolMaxSize)
            mMsgPoolIsFull = true;
    }

    //------------------------------------------------------------------------//
    void MsgQueue::clearMsgPool(void)
    {
        AutoMutex critical((Mutex* const)&mMsgPoolMutex);

        Message pre;
        while (mMsgPool)
        {
            pre = std::move(mMsgPool);
            mMsgPool = std::move(pre->mNext);
            pre.reset();
        }

        mMsgPoolSize = 0;
    }

   //------------------------------------------------------------------------//
    void MsgQueue::quit(bool safely/* = true*/)
    {
        AutoMutex critical(&mLock);
        
        if(mQuit)
            return;

        if (safely)
        {
            mNotEnqueMsg = true;
            mBlocked = false;
            mWait.notifyAll();
            return;
        }

        // does not use reset to release list, it maybe stack overflow
        // when the mMsgQueue'size too large
//        mMsgQueue.reset();

        Message pre;
        while (mMsgQueue)
        {
            pre = std::move(mMsgQueue);
            mMsgQueue = std::move(pre->mNext);
            pre.reset();
        }

        mQuit = true;
        mBlocked = false;
        mWait.notifyAll();
    }

   //------------------------------------------------------------------------//
    void MsgQueue::setTestOutTimeMillisExit(long t)
    {
        AutoMutex critical(&mLock);
        mOutTimeTest = t;
    }

   //------------------------------------------------------------------------//
    void MsgQueue::addIdleHandler(const msgQueueIdleHandler& handler)
    {
        AutoMutex critical(&mLock);
        mIdleHandlerFunc = handler;
    }    

   //------------------------------------------------------------------------//
    void MsgQueue::removeIdleHandler(void)
    {
        AutoMutex critical(&mLock);
        mIdleHandlerFunc = nullptr;
    }

   //------------------------------------------------------------------------//
    void MsgQueue::dumpQueueList(void) const
    {
        AutoMutex critical(&mLock);
        Msg* p = mMsgQueue.get();
        printf("\n");
        LOGI("%s\n\n", "---------------------Message queue begin---------------------");
        for(;;)
        {
            if (p)
            {
                std::string s;
                if (p->mNext.get())
                    s = "Message  of queue shared_ptr = %p, what = %d, when = %llu, Using = %s";
                else
                    s = "Message  of queue shared_ptr = %p, what = %d, when = %llu, Using = %s\n";
                LOGI(s.c_str(), p, p->mWhat, p->mWhen, p->isInUse() ? "true" : "false");
                p = p->mNext.get();       
            }
            else    
                break;
        }

        LOGI("%s\n", "----------------------Message queue end----------------------");
    }   

   //------------------------------------------------------------------------//
    void MsgQueue::dumpQueuePool(void) const
    {
        AutoMutex critical((Mutex* const)&mMsgPoolMutex);
        Msg* p = mMsgPool.get();
        printf("\n");
        LOGI("%s\n\n", "---------------------Message pool begin----------------------");
        for(;;)
        {
            if (p)
            {
                std::string s;
                if (p->mNext.get())
                    s = "Message of pool shared_ptr = %p, what = %d, when = %llu, Using = %s";
                else
                    s = "Message of pool shared_ptr = %p, what = %d, when = %llu, Using = %s\n";
                LOGI(s.c_str(), p, p->mWhat, p->mWhen, p->isInUse() ? "true" : "false");
                p = p->mNext.get();       
            }
            else
                break;
        }

        LOGI("%s\n", "----------------------Message pool end-----------------------");
    }

__END__
