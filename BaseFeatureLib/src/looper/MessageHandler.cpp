#include "../../inc/looper/MessageHandler.h"
#include "../../inc/looper/MessageLooper.h"
#include "../../inc/looper/MessageQueue.h"
#include "../../inc/base/TimeUtil.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../inc/os/Logger.h"

//---------------------------------------------------------------------------------------//
__BEGIN__
    
   //------------------------------------------------------------------------------------//
    #ifdef LOG_TAG
        #undef LOG_TAG
    #endif
    #define LOG_TAG (MessgeHandler):

   //------------------------------------------------------------------------------------//
    MsgHandler::MsgHandler(void)
    : mLooper(nullptr)
    , mQueue(nullptr)
    , mCallback(nullptr)
    , mMessageHandlerFn(nullptr)
    , mmsgCallbackObj(nullptr)
    , mContext(nullptr)
    , mMutex(nullptr)
    { 
        mMutex = new Mutex(PTHREAD_MUTEX_RECURSIVE_NP);
    }

   //------------------------------------------------------------------------------------//
    MsgHandler::~MsgHandler(void)
    {
        mCallback = nullptr;
        mMessageHandlerFn = nullptr;
        mmsgCallbackObj = nullptr;
        mContext = nullptr;
        delete mMutex;
        mMutex = nullptr;

        LOGD("%s", "Handler been destroyed!");
    }

   //------------------------------------------------------------------------------------//
    Handler MsgHandler::createHandler(void* context/* = nullptr */)
    { 
        Handler h =  Handler(new MsgHandler(), deleter<MsgHandler>());
        h->mLooper = MsgLooper::myLooper();
        h->mQueue = h->mLooper->getMsgQueue();
        h->mContext = context;
        return h;
    }

   //------------------------------------------------------------------------------------//
    Handler MsgHandler::createHandler(const messageCallback& callback, void* context/* = nullptr */)
    { 
        Handler h = createHandler(context);
        h->mCallback = callback;
        return h;
    }

   //------------------------------------------------------------------------------------//
    Handler MsgHandler::createHandler(const Looper& looper, void* context/* = nullptr */)
    { 
        Handler h = Handler(new MsgHandler(), deleter<MsgHandler>());
        h->mLooper = looper;
        h->mQueue = h->mLooper->getMsgQueue();
        h->mContext = context;
        return h;
    }

   //------------------------------------------------------------------------------------//
    Handler MsgHandler::createHandler(const Looper& looper, const messageCallback& callback, void* context/* = nullptr */)
    {
        Handler h = createHandler(looper, context);
        h->mCallback = callback;
        return h;
    }

   //------------------------------------------------------------------------------------//
    messageCallback MsgHandler::getCallback(void) const
    {
        return mCallback;
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::post(const runnable& r)
    {
        sendMessageDelayed(Msg::obtain(r, Handler(this, deleter<MsgHandler>())), 0);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::post(const runnable& r, long delayMillis)
    {
        sendMessageDelayed(Msg::obtain(r, Handler(this, deleter<MsgHandler>())), delayMillis);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendMessage(Message msg)
    {
        sendMessageDelayed(std::move(msg), 0);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendEmptyMessage(int what)
    {
        sendMessageDelayed(Msg::obtain(what, Handler(this, deleter<MsgHandler>())), 0);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendEmptyMessage(int what, long delayMillis)
    {
        sendMessageDelayed(Msg::obtain(what, Handler(this, deleter<MsgHandler>())), delayMillis);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::postAtTime(Message msg, long uptimeMillis)
    {
        sendMessageAtTime(std::move(msg), uptimeMillis);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::postAtTime(const runnable& r, long uptimeMillis)
    {
        sendMessageAtTime(Msg::obtain(r, Handler(this, deleter<MsgHandler>())), uptimeMillis);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendMessageDelayed(Message msg, long delayMillis)
    {
        // No lock required
        if(delayMillis < 0)
            delayMillis = 0;

        uint64 t = getNowTimeOfNs() / PER_SEC_USEC;
        sendMessageAtTime(std::move(msg), t + delayMillis);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendMessageAtFrontOfQueue(Message msg)
    {
        sendMessageAtTime(std::move(msg), 0);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::setMsgHandlerFunc(const messageHandlerFunc& fn)
    {
        AutoMutex critical(mMutex);
        mMessageHandlerFn = fn;
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::setMsgCallbackObject(const HandlerCallback* callbackObj)
    {
        AutoMutex critical(mMutex);
        mmsgCallbackObj = const_cast<HandlerCallback*>(callbackObj);
    }
    
   //------------------------------------------------------------------------------------//
    bool MsgHandler::hasMessage(const Message& msg)
    {
        return mQueue->hasMessage(msg, this);
    }

   //------------------------------------------------------------------------------------//
    bool MsgHandler::hasMessage(const runnable& r)
    {
        return mQueue->hasMessage(r, this);
    }

   //------------------------------------------------------------------------------------//
    bool MsgHandler::hasMessage(int what)
    {
        return mQueue->hasMessage(what, this);
    }

   //------------------------------------------------------------------------------------//
    bool MsgHandler::hasMessage(const HandlerCallback* callback)
    {
        return mQueue->hasMessage(callback, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(runnable& r)
    {
        mQueue->removeMessage(r, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int what)
    {
        mQueue->removeMessage(what, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int minWhat, int maxWhat, messageCallback& c)
    {
        mQueue->removeMessage(minWhat, maxWhat, c, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int what, int arg1, int arg2, messageCallback& c)
    {
        mQueue->removeMessage(what, arg1, arg2, c, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(HandlerCallback* callback)
    {
        mQueue->removeMessage(callback, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int what, HandlerCallback *callback)
    {
        mQueue->removeMessage(what, callback, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int minWhat, int maxWhat, HandlerCallback* callback)
    {
        mQueue->removeMessage(minWhat, maxWhat, callback, this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeMessage(int what, int arg1, int arg2, HandlerCallback* callback)
    {
        mQueue->removeMessage(what, arg1, arg2, callback, this);        
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::removeAllMessages(void)
    {
        mQueue->removeAllMessages(this);
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::dispatchMessage(const Message& msg)
    {
        if(msg == nullptr)
        {
            LOGW("%s", "message is null");
            return;
        }
        
        if(msg->mCallback)
            msg->mCallback(msg, mContext);
        else if(msg->mHandleCallback)
            msg->mHandleCallback->onHandler(msg);
        else
        {
            // presume your do not change the callback when this function is doing
//          AutoMutex critical(mMutex);

            // defalut callback of handler
            if (mCallback)
                mCallback(msg, mContext);
            else
            {
                // message handle
                if (mMessageHandlerFn)
                    mMessageHandlerFn(msg, mContext);
                else if(mmsgCallbackObj)
                    mmsgCallbackObj->onHandler(msg);
            }
        }    
    }

   //------------------------------------------------------------------------------------//
    void MsgHandler::sendMessageAtTime(Message msg, uint64 uptimeMillis)
    {
        msg->mTarget = this;
        mQueue->enqueueMessage(std::move(msg), uptimeMillis);
    }

__END__
