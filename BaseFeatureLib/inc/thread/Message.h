#ifndef __Message_h__
#define __Message_h__
#include "../base/Uncopyable.hpp"
#include "../os/AutoMutex.hpp"
#include <memory>
#include <typeinfo>

//---------------------------------------------------------------------------------------//
__BEGIN__
  
    //------------------------------------------------------------------------------------//
    #ifndef make_unique
    template<typename T, typename ...Args>
    std::unique_ptr<T> make_unique(Args&& ...args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
    #endif

    //------------------------------------------------------------------------------------//
    class Msg;
    class MsgHandler;
    class MsgLooper;
    class MsgQueue;

    typedef std::unique_ptr<Msg, deleter<Msg>>    Message;
    typedef std::shared_ptr<MsgHandler>                 Handler;
    typedef std::shared_ptr<MsgQueue>                   Queue;
    typedef std::shared_ptr<MsgLooper>                  Looper;

    typedef void (*messageCallback)(const Message& msg, void* context);
    typedef void (*paramDeleter)(void* obj, size_t bytes);
    typedef messageCallback messageHandlerFunc;
    typedef messageCallback runnable;
    typedef messageCallback msgQueueIdleHandler;

    //------------------------------------------------------------------------------------//
    class HandlerCallback
    {
        public:
            HandlerCallback(void) = default;
            virtual ~HandlerCallback(void) { }
            virtual void onHandler(const Message& msg) = 0;
    };

    //------------------------------------------------------------------------------------//
    // Note: message is not thread safety. I assume you don't using same message
    // in multithreads
    class API_EXPORTS Msg : private Uncopyable
    {
        friend class MsgQueue;
        friend struct deleter<Msg>;
        public:
            Msg(const Msg& msg) = delete;
            Msg(Msg&& msg) = delete;
            Msg& operator =(const Msg& msg) = delete;
            Msg& operator =(Msg&& msg) = delete;

            // set parameters of message
            void setParam(const void* param, size_t bytes, const paramDeleter& freeFn = nullptr)
            { mParam = const_cast<void*>(param); mParamBytes = bytes; mParamFreeFunc = freeFn; }

            void* getParam(void) const { return mParam; }
            size_t ParamSize(void) const { return mParamBytes; }
            paramDeleter getParamDeleter(void) const { return mParamFreeFunc; }
            
            bool isInUse(void);

            void makeInUse(void);

            void recycle(void);

            void recycleUnchecked(void);

            // obtain a new message
            static Message obtain(void);

            static Message obtain(const Handler& h);

            static Message obtain(const runnable& r, const Handler& h = Handler(nullptr));

            static Message obtain(int what, const Handler& h = Handler(nullptr));

            static Message obtain(int what, int arg1, int arg2, const Handler& h = Handler(nullptr));

            static Message obtain(int what, int arg1, int arg2, const void* param, size_t bytes, const paramDeleter& freeFn, const Handler& h = Handler(nullptr));

            static Message obtain(int what, int arg1, int arg2, const HandlerCallback* callback, const void* param, size_t bytes, const paramDeleter& freeFn, const Handler& h = Handler(nullptr));

        private:
            Msg(void);
            ~Msg(void);

        public:
            int                           mWhat;
            int                           mArg1;
            int                           mArg2;
            runnable                 mCallback;
            HandlerCallback*   mHandleCallback;
            MsgHandler*         mTarget;
            uint64                     mWhen;
            int                           mFlags;
            Message                 mNext;
            
        private:
            static int                  FLAGINUSE;
            static int                  FLAGASYNC;
            // The 3 paramete  r are private, which purpose is avoiding forgetting to set  
            void*                      mParam;
            size_t                      mParamBytes;
            paramDeleter         mParamFreeFunc;
    };

__END__

#endif // __Message_h__
