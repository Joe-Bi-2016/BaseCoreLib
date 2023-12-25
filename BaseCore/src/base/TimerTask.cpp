/*****************************************************************************
* FileName    : TimerTask.cpp
* Description : Task timer implemention
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/base/TimerTask.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../inc/looper/MessageLooper.h"

//---------------------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------------------//
    Mutex TimerTask::gMutex;
    threadlocal Looper  TimerTask::mLoop = nullptr;

    //------------------------------------------------------------------------------------//
    enum Timer_Message_Type
    {
        TIMER_MESSAGE = 0,
    };
   
    //------------------------------------------------------------------------------------//
    TimerTask::TimerTask()
    : mHandler(nullptr)
    {
        if (!mLoop)
        {
            AutoMutex lockGuard(&gMutex);
            if(!mLoop)
                mLoop = MsgLooper::prepare();
        }

        mHandler = MsgHandler::createHandler();
        mLoop->loop();
    }
    
    //------------------------------------------------------------------------------------//
    TimerTask::~TimerTask()
    {
        if(mLoop)
            mLoop->quit();
    }
    
    //------------------------------------------------------------------------------------//
    void TimerTask::schedule(TimerTaskFunc * taskfunc, int timeout)
    {
        sendTimerTaskMsg(TIMER_MESSAGE, 0, 0, timeout, taskfunc);
    }
    
    //------------------------------------------------------------------------------------//
    void TimerTask::sendTimerTaskMsg(int what, int arg1, int arg2 /* = 0 */, int timeout /* = 0 */, void *data /* = nullptr */, size_t bytes /* = 0 */, const paramDeleter& fnFree /* = nullptr */, bool isRemoved/* = false */)
    {
        if (isRemoved)
            mHandler->removeMessage(what);
    
        Message msg = Msg::obtain(what, arg1, arg2, data, bytes, fnFree, mHandler);
        mHandler->sendMessageDelayed(std::move(msg), timeout);
        mHandler->setMsgHandlerFunc(onTimerTaskFuncHandler);
    }
    
    //------------------------------------------------------------------------------------//
    void onTimerTaskFuncHandler(const Message& msg, void* context)
    {
        switch (msg->mWhat)
        {
            case TIMER_MESSAGE:
            {
                void *p = msg->getParam();
                if (p)
                {
                    TimerTaskFunc *timerTask = (TimerTaskFunc *)p;
                    timerTask->onResponse();
                }
            }
            break;
    
            default:
            break;
        }
    }

__END__
