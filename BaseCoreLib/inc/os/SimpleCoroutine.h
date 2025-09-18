/*****************************************************************************
* FileName    : SimpleCoroutine.h
* Description : Simple coroutine 
* Author      : Joe.Bi
* Date        : 2025-07
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __SimpleCoroutine_h__
#define __SimpleCoroutine_h__
#include "../base/Macro.h"
#include <stdint.h>
#include <setjmp.h>

//---------------------------------------------------------------------------//
__BEGIN__
__CExternBegin__

    //-----------------------------------------------------------------------//
    enum costatus {
        co_ready = 0,
        co_running,
        co_suspend,
        co_done,
    };

    typedef void* (*co_func)(void* __arg__);
    
    struct coro {
        co_func func;
        void* arg;
        uint8_t* stack;
        jmp_buf ctx;
        enum costatus status;
        struct coro* next;
    };
    
    struct coro* coro_create(co_func __cofunc__, void* __arg__);
    void coro_resume(struct coro* __coro__);
    void coro_yield(void);
    void coro_destroy(struct coro* __coro__);

    __CExternEnd__
__END__

#endif // __SimpleCoroutine_h__

