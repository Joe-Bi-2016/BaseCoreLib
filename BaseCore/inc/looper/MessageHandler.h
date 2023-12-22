/*****************************************************************************
* FileName      : MessageHandler.h
* Description   : Message handler definition
* Author           : Joe.Bi
* Date              : 2023-12
* Version         : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __MessageHandler_h__
#define __MessageHandler_h__
#include "Message.h"
#include "../os/Mutex.hpp"

//---------------------------------------------------------------------------------------//
__BEGIN__
    
    //------------------------------------------------------------------------------------//
    class API_EXPORTS MsgHandlerFunc
    {
        public:
            MsgHandlerFunc() = default;
            virtual ~MsgHandlerFunc(void) { }
            virtual void operator()(const Message& msg) { }
    };

   //------------------------------------------------------------------------------------//
    class API_EXPORTS MsgHandler : private Uncopyable
    {
        friend struct deleter<MsgHandler>;
        friend class Msg;

        public:
            static Handler createHandler(void* context = nullptr);

            static Handler createHandler(const messageCallback& callback, void* context = nullptr);

            static Handler createHandler(const Looper& looper, void* context = nullptr);

            static Handler createHandler(const Looper& looper, const messageCallback& callback, void* context = nullptr);

            messageCallback getCallback(void) const;

            void post(const runnable& r);

            void post(const runnable& r, long delayMillis);

            void sendMessage(Message msg);

            void sendEmptyMessage(int what);

            void sendEmptyMessage(int what, long delayMillis);

            void postAtTime(Message msg, long uptimeMillis);

            void postAtTime(const runnable& r, long uptimeMillis);

            void sendMessageDelayed(Message msg, long delayMillis);

            void sendMessageAtFrontOfQueue(Message msg);

            void setMsgHandlerFunc(const messageHandlerFunc& fn);

            void setMsgCallbackObject(const HandlerCallback* callbackObj);

            bool hasMessage(const Message& msg);

            bool hasMessage(const runnable& r);

            bool hasMessage(int what);

            bool hasMessage(const HandlerCallback* callback);

            void removeMessage(runnable& r);

            void removeMessage(int what);

            void removeMessage(int minWhat, int maxWhat, messageCallback& c);

            void removeMessage(int what, int arg1, int arg2, messageCallback& c);

            void removeMessage(HandlerCallback* callback);

            void removeMessage(int what, HandlerCallback *callback);

            void removeMessage(int minWhat, int maxWhat, HandlerCallback* callback);

            void removeMessage(int what, int arg1, int arg2, HandlerCallback* callback);

            void removeAllMessages(void);

            void dispatchMessage(const Message& msg);
        
        private:
            MsgHandler(void);
            ~MsgHandler(void);

            void sendMessageAtTime(Message msg, uint64 uptimeMillis);

        private:
            Looper                          mLooper;
            Queue                           mQueue;
            messageCallback          mCallback;          // defalut message handler function
            messageHandlerFunc   mMessageHandlerFn;  // user messge handler function
            HandlerCallback*          mmsgCallbackObj;
            void*                             mContext;
            mutable Mutex*           mMutex;
    };

__END__

 #endif // __MessageHandler_h__
