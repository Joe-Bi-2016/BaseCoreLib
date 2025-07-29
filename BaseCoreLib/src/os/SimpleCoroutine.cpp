/*****************************************************************************
* FileName    : SimpleCoroutine.cpp
* Description : Simple coroutine implementation 
* Author      : Joe.Bi
* Date        : 2025-07
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#include "../../inc/os/SimpleCoroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>

//---------------------------------------------------------------------------//
__BEGIN__
__CExternBegin__
    
    //-----------------------------------------------------------------------//
    #define STACK_ALIGNMENT 16
    #define STACK_SIZE 8192
    
    static struct coro* g_co_list = NULL;
    static struct coro g_main_co = { NULL, NULL, NULL, {0}, co_running, NULL };
    static struct coro* g_cur_co = &g_main_co;
    static int g_co_cnt = 0;

    //-----------------------------------------------------------------------//    
    static inline uintptr_t alignstack(uintptr_t stack, size_t alignment) {
        return ((stack + alignment - 1) & ~(alignment - 1));
    }
    
    //-----------------------------------------------------------------------//
    void __co_add_list(struct coro* __coro__) {
        struct coro* head = g_co_list;
        while (head) {
            if (head == __coro__)
                return;
            head = head->next;
        }
        __coro__->next = g_co_list;
        g_co_list = __coro__;
        g_co_cnt++;
    }
    
    //-----------------------------------------------------------------------//
    struct coro* __co_select(void) {
        if (g_co_list == NULL)
            return NULL;
    
        int idx = rand() % g_co_cnt;
        struct coro* cur = g_co_list;
        while (idx-- > 0) {
            cur = cur->next;
        }
    
        return   cur;
    }
    
    //-----------------------------------------------------------------------//
    struct coro* __co_create(co_func __cofunc__, void* __arg__) {
        struct coro* new_co = (struct coro*)malloc(sizeof(struct coro));
        if (!new_co)
            return NULL;
    
        memset(new_co, 0x0, sizeof(struct coro));
        new_co->func = __cofunc__;
        new_co->arg = __arg__;
        new_co->status = co_ready;
        new_co->next = NULL;
    
        return new_co;
    }
    
    //-----------------------------------------------------------------------//
    #if defined(_MSC_VER)
    void __stdcall __co_run(struct coro* __coro__) {
    #else 
    void __co_run(struct coro* __coro__) {
    #endif
        assert(__coro__ != NULL);
        __coro__->func(__coro__->arg);
        __coro__->status = co_done;
        g_cur_co = &g_main_co;
        longjmp(g_cur_co->ctx, 1);
    }
    
    //-----------------------------------------------------------------------//
    // Note: __co_resume can only be called through the main coroutine, otherwise it will destroy g_main_co  
    void __co_resume(struct coro* __coro__) {
        assert(__coro__ != NULL);
        assert(__coro__->func && __coro__->arg);
    
        if (__coro__->status != co_ready && __coro__->status != co_suspend)
            return;
    
        // malloc memory for corountine stack memory
        if (__coro__->stack == NULL) {
            __coro__->stack = (uint8_t*)malloc(STACK_SIZE);
            if (__coro__->stack == NULL) {
                free(__coro__);
                return;
            }
        }
    
        __co_add_list(&g_main_co);
        __co_add_list(__coro__);
    
        if (setjmp(g_main_co.ctx) == 0) {
            g_cur_co = __coro__;
            g_cur_co->status = co_running;
            void* func = __co_run;
            void* arg = g_cur_co;
    
            void* stack = (void*)alignstack((uintptr_t)g_cur_co->stack + STACK_SIZE, STACK_ALIGNMENT);
            assert(((uintptr_t)stack & 0xF) == 0);
    
    #if defined(__GNUC__) || defined(__clang__)
            // format: asm volatile("InSTructiON List" : Output: Input: Clobber / Modify)
    #if defined(__x86_64__)
            asm volatile(
                "movq %0, %%rsp;"
                "subq $0x20, %%rsp;"
                "movq %1, %%rdi;"
                "call  *%2;"
                :
            : "r"(stack), "r"(arg), "r"(func)
                : "rdi", "rsp", "memory");
    #else
            asm volatile(
                "movq %0, %%rsp;"
                "subq $8, %%rsp;"
                "movq %1, %%rdi;"
                "call  *%2;"
                :
            : "r"(stack), "r"(arg), "r"(func)
                : "rdi", "rsp", "memory");
    #endif
    #elif defined(_MSC_VER)
    #if defined(_WIN64)
            __asm {
                mov rsp, stack
                sub rsp, 0x20
                mov rcx, arg
                call func
            }
    #else
            __asm {
                mov esp, stack
                sub esp, 8
                push arg
                call func
            }
    #endif
    #else
    #error "Unsupported compiler"
    #endif
        }
    }
    
    //-----------------------------------------------------------------------//
    void __co_yield(void) {
        assert(g_cur_co != NULL);
        int ret = setjmp(g_cur_co->ctx);
        if (ret == 0) {
            struct coro* next = __co_select();
            assert(next != NULL);
            if (next == NULL || next == g_cur_co || next->status == co_done) {
                longjmp(g_cur_co->ctx, 1);
                return;
            }
    
            if (g_cur_co->status == co_running)
                g_cur_co->status = co_suspend;
    
            g_cur_co = next;
            if (g_cur_co->status == co_suspend)
                g_cur_co->status = co_running;
    
            longjmp(g_cur_co->ctx, 1);
        }
    }
    
    //-----------------------------------------------------------------------//
    void __co_finish(struct coro* __coro__) {
        assert(__coro__ != NULL);
    
        if (__coro__->status == co_ready) {
            printf("The coroutine has not been started. If it is not resumed, there will be a memory leak!\n");
            return;
        }
    
        while (__coro__->status != co_done)
            __co_yield();
    
        struct coro* pre = NULL;
        struct coro* cur = g_co_list;
        while (cur != NULL) {
            if (cur == __coro__) {
                if (pre)
                    pre->next = cur->next;
                else
                    g_co_list = cur->next;
                cur->next = NULL;
                free(cur->stack);
                free(cur);
                g_co_cnt--;
                break;
            }
            pre = cur;
            cur = cur->next;
        }
    }


    __CExternEnd__
__END__
