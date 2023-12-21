/*****************************************************************************
* Description: message handler implemention.
* Author     : Bi ShengWang(shengwang.bisw@alibaba-inc.com.)
* Date       : 2020.06.03
* Copyright (c) alibaba All rights reserved.
******************************************************************************/
#include "../../inc/base/TimerTask.h"
#include "../../inc/os/AutoMutex.hpp"
#include "../../inc/thread/MessageLooper.h"

//---------------------------------------------------------------------------------------//
__BEGIN__

    //------------------------------------------------------------------------------------//
    thread_local Mutex TimerTask::gMutex;
    thread_local Looper  TimerTask::mLoop = nullptr;

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
