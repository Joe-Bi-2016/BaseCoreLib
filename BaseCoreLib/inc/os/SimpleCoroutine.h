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
/*
 * IMPORTANT NOTE:
 * This coroutine library uses setjmp/longjmp for context switching.
 * setjmp does NOT preserve floating-point or vector registers on most platforms.
 * If your coroutine functions perform floating-point operations or use SIMD,
 * the state of those registers may be corrupted after a yield/resume cycle.
 * Use only integer and general-purpose registers inside coroutines, or explicitly
 * save/restore FPU state if your platform supports it (e.g., sigsetjmp).
 */

//---------------------------------------------------------------------------//
__BEGIN__
__CExternBegin__

    //-----------------------------------------------------------------------//
	typedef enum {
		co_ready = 0,
		co_running ,
		co_suspend,
		co_done,
	}costatus;
	
	// erro code
	#define CO_ERR_NONE  0
	#define CO_ERR_INVAL 1
	#define CO_ERR_NOMEM 2

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
	void coro_init(unsigned int seed);
	
	// utility function
	typedef struct coro* (*coro_scheduler_t)(void);	
	
	costatus coro_status(struct coro* __coro__);
	int coro_errno(void);
	const char* coro_errmsg(void);
	void coro_set_scheduler(coro_scheduler_t sched);

    __CExternEnd__
__END__

#endif // __SimpleCoroutine_h__

