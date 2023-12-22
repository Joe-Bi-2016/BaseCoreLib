/*****************************************************************************
* FileName      : Message.cpp
* Description   : Message implemention
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/looper/Message.h"
#include "../../inc/looper/MessageHandler.h"
#include "../../inc/looper/MessageQueue.h"
#include "../../inc/looper/MessageLooper.h"
#include "../../inc/os/AutoMutex.hpp"
#include <assert.h>

//---------------------------------------------------------------------------------------//
__BEGIN__
    
    int Msg::FLAGINUSE   = 1 << 0;
    int Msg::FLAGASYNC  = 1 << 1;
    
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (Message):

   //------------------------------------------------------------------------------------//
    Msg::Msg(void)
    : mWhat(0)
    , mArg1(0)
    , mArg2(0)
    , mCallback(0)
    , mHandleCallback(0)
    , mTarget(nullptr)
    , mWhen(0)
    , mFlags(0)
    , mNext(nullptr)
    , mParam(0)
    , mParamBytes(0)
    , mParamFreeFunc(0)  
    { 
    }

   //------------------------------------------------------------------------------------// 
    Msg::~Msg(void)
    {
        recycleUnchecked();
        LOGD("%s", "Message been destroyed!");
    }

   //------------------------------------------------------------------------------------//
    bool Msg::isInUse(void)
    {
        return ((mFlags & FLAGINUSE) == FLAGINUSE);
    }  

   //------------------------------------------------------------------------------------//
    void Msg::makeInUse(void)
    {
        mFlags |= FLAGINUSE;
    }  

   //------------------------------------------------------------------------------------//
    void Msg::recycle(void)
    {
        if(isInUse())
        {
           LOGE("%s", "message is being using, CAN'T BEING RECYCLE!!!");     
           return;
        }

        recycleUnchecked();
    } 

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(void)
    {
        Queue q = MsgLooper::myLooper()->getMsgQueue();
        Message m = q ? q->obtain() : Message(nullptr);
       
        if(m.get() == nullptr)
            m = Message(new Msg());

        return m;
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(const Handler& h)
    {
        if(h.get() == nullptr)
        {
            LOGW("%s", "handler is null, create a new message");
            return obtain();
        }
        else
        {
            Message m = h->mQueue ? h->mQueue->obtain() : Message(nullptr);
            if(m.get() == nullptr)
            {
                LOGW("%s", "Message pool queue is not full or empty, create a new message");
                return Message(new Msg());
            }

            return m;
        }    
        
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(const runnable& r, const Handler& h/* = Handler(nullptr) */)
    {
        Message m = obtain(h);
        m->mCallback = r; 
        return m;     
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(int what, const Handler& h/* = Handler(nullptr) */)
    {
        Message m = obtain(h);
        m->mWhat = what;
        return m;
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(int what, int arg1, int arg2, const Handler& h/* = Handler(nullptr) */)
    {
        Message m = obtain(h);
        m->mWhat = what;
        m->mArg1 = arg1;
        m->mArg2 = arg2;
        return m;
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(int what, int arg1, int arg2, const void* param, size_t bytes, const paramDeleter& freeFn, const Handler& h/* = Handler(nullptr) */)
    {
        Message m = obtain(h);
        m->mWhat = what;
        m->mArg1 = arg1;
        m->mArg2 = arg2;
        m->mParam = const_cast<void*>(param);
        m->mParamBytes = bytes;
        m->mParamFreeFunc = freeFn;

        return m;
    }

   //------------------------------------------------------------------------------------//
    Message Msg::obtain(int what, int arg1, int arg2, const HandlerCallback* callback, const void* param, size_t bytes, const paramDeleter& freeFn, const Handler& h/* = Handler(nullptr) */)
    {
        Message m = obtain(h);
        m->mWhat = what;
        m->mArg1 = arg1;
        m->mArg2 = arg2;
        m->mHandleCallback = const_cast<HandlerCallback*>(callback);
        m->mParam = const_cast<void*>(param);
        m->mParamBytes = bytes;
        m->mParamFreeFunc = freeFn;

        return m;
    }

   //------------------------------------------------------------------------------------//
    void Msg::recycleUnchecked(void)
    {
        if (mParamFreeFunc)
        {
            mParamFreeFunc(mParam, mParamBytes);
            mParam = nullptr;
        }

        mWhat = 0;
        mArg1 = 0;
        mArg2 = 0;
        mCallback = nullptr;
        mHandleCallback = nullptr;
        mTarget = nullptr;
        mWhen = 0;
        mFlags = 0;
        mParamBytes = 0;
        mParamFreeFunc = nullptr;
    }   
     
__END__
