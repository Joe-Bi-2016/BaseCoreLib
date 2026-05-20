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
#include <memory.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

//---------------------------------------------------------------------------//
__BEGIN__
__CExternBegin__
    
    //-----------------------------------------------------------------------//
    #define STACK_ALIGNMENT 16
    #define STACK_SIZE 16384
    
    static threadlocal struct coro* g_co_list = NULL;
    static threadlocal struct coro g_main_co = { NULL, NULL, NULL, {0}, co_running, NULL };
    static threadlocal struct coro* g_cur_co = &g_main_co;
    static threadlocal int g_co_cnt = 0;
	static threadlocal int g_co_errno = CO_ERR_NONE;
	static threadlocal const char* g_co_errmsg = NULL;
	static threadlocal coro_scheduler_t g_scheduler = NULL;

	//-----------------------------------------------------------------------//
	costatus coro_status(struct coro* __coro__) {
		return __coro__ ? __coro__->status : co_done;
	}

	//-----------------------------------------------------------------------//
	int coro_errno(void) {
		return g_co_errno;
	}

	//-----------------------------------------------------------------------//
	const char* coro_errmsg(void) {
		return g_co_errmsg ? g_co_errmsg : "no error";
	}

	//-----------------------------------------------------------------------//
	void coro_set_scheduler(coro_scheduler_t sched) {
		g_scheduler = sched;
	}

	//-----------------------------------------------------------------------//
	/* stack alloc memory function.
	 * Use mmap/VirtualAlloc to allocate an independent stack, 
	 * and place a guard page at the bottom to prevent overflow
	 */
	static uint8_t* stack_alloc(size_t total_size) {
	#if (defined(_WIN32) || defined(_WIN64))
		LPVOID mem = VirtualAlloc(NULL, total_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (!mem) {
			g_co_errno = CO_ERR_NOMEM;
			g_co_errmsg = "VirtualAlloc failed";
			return NULL;
		}
		return (uint8_t*)mem;
	#else
		void* mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (mem == MAP_FAILED) {
			g_co_errno = CO_ERR_NOMEM;
			g_co_errmsg = "mmap failed";
			return NULL;
		}
		return (uint8_t*)mem;
	#endif
	}
	
	//-----------------------------------------------------------------------//
	static void stack_free(uint8_t* stack, size_t total_size) {
		if (!stack) return;
	#if (defined(_WIN32) || defined(_WIN64))
		VirtualFree(stack, 0, MEM_RELEASE);
	#else
		munmap(stack, total_size);
	#endif
	}
	
	//-----------------------------------------------------------------------//
	// set up a protected page, with the first page having no access rights
	static void stack_protect_first_page(uint8_t* stack, size_t page_size) {
	#if (defined(_WIN32) || defined(_WIN64))
		DWORD oldprot;
		VirtualProtect(stack, page_size, PAGE_NOACCESS, &oldprot);
	#else
		mprotect(stack, page_size, PROT_NONE);
	#endif
	}
	
	//-----------------------------------------------------------------------//
	static inline size_t get_page_size(void) {
	#if (defined(_WIN32) || defined(_WIN64))
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return si.dwPageSize;
	#else
		return (size_t)sysconf(_SC_PAGESIZE);
	#endif
	}

    //-----------------------------------------------------------------------//    
    static inline uintptr_t alignstack(uintptr_t stack, size_t alignment) {
        return ((stack + alignment - 1) & ~(alignment - 1));
    }
    
    //-----------------------------------------------------------------------//
    void coro_add_list(struct coro* __coro__) {
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
    static struct coro* default_scheduler(void) {
       if (g_co_list == NULL)
            return NULL;

        static threadlocal int cnt = 0;
        int idx = rand() % g_co_cnt;
        struct coro* cur = g_co_list;
        while (idx-- > 0) {
            cur = cur->next;
        }

        cnt++;
		
        if (cur->status == co_done) { // if cnt == 2, then reback to main coroutine to running
            if (cnt == 2) {
                cur = &g_main_co;
            } else {
				cur = default_scheduler();
			}
        }
		
        cnt--;

        return cur;
    }
    
	//-----------------------------------------------------------------------//
	void coro_init(unsigned int seed) {
		srand(seed);
	}

    //-----------------------------------------------------------------------//
    struct coro* coro_create(co_func __cofunc__, void* __arg__) {
        struct coro* new_co = (struct coro*)malloc(sizeof(struct coro));
        if (!new_co) {
			g_co_errno = CO_ERR_NOMEM;
			g_co_errmsg = "failed to allocate coroutine structure";
			return NULL;
		}
    
        memset(new_co, 0x0, sizeof(struct coro));
        new_co->func = __cofunc__;
        new_co->arg = __arg__;
        new_co->status = co_ready;
        new_co->next = NULL;
    
        return new_co;
    }
    
    //-----------------------------------------------------------------------//
    #if defined(_MSC_VER)
    void __stdcall coro_run(struct coro* __coro__) {
    #else 
    void coro_run(struct coro* __coro__) {
    #endif
        if (!__coro__) return;
        __coro__->func(__coro__->arg);
        __coro__->status = co_done;
        coro_yield();
    }
    
    //-----------------------------------------------------------------------//
    // Note: coro_resume can only be called through the main coroutine, otherwise it will destroy g_main_co  
    void coro_resume(struct coro* __coro__) {
		if (!__coro__ || !__coro__->func || !__coro__->arg) {
			g_co_errno = CO_ERR_INVAL;
			g_co_errmsg = "invalid coroutine (NULL or missing func/arg)";
			return;
		}

		if (__coro__->status != co_ready && __coro__->status != co_suspend) {
			g_co_errno = CO_ERR_INVAL;
			g_co_errmsg = "coroutine is not in a resumable state";
			return;
		}	
    
        // malloc memory for corountine stack memory
        if (__coro__->stack == NULL) {
            size_t page_size = get_page_size();
			size_t total_size = STACK_SIZE + page_size; 
			__coro__->stack = stack_alloc(total_size);
			if (!__coro__->stack) {
				return;
			}
			
			stack_protect_first_page(__coro__->stack, page_size);
        }
    
		if (g_co_cnt == 0) // a main coroutine object can only be added to the list once
			coro_add_list(&g_main_co);
			
        coro_add_list(__coro__);
    
        if (setjmp(g_main_co.ctx) == 0) {
            g_cur_co = __coro__;
			if (g_cur_co->status == co_suspend) {// avoid restart __coro__'s function
				longjmp(g_cur_co->ctx, 1);
				return;
			}

            g_cur_co->status = co_running;
            void* func = coro_run;
            void* arg = g_cur_co;
    
			size_t page_size = get_page_size();
			uint8_t* usable_stack = __coro__->stack + page_size;
			void* stack = (void*)alignstack((uintptr_t)usable_stack + STACK_SIZE, STACK_ALIGNMENT);

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
			#elif defined(__i386__)
            asm volatile(
				"movl %0, %%esp;"
				"subl $8, %%esp;"
				"pushl %1;"
				"call *%2;"
				"addl $4, %%esp;"
				:
				: "r"(stack), "r"(arg), "r"(func)
				: "eax", "ecx", "edx", "memory");
			#elif defined(__aarch64__) || defined(__arm64__)
            /* AArch64 (ARM64) - GCC/Clang
               Set sp to coroutine stack, prepare x0 (first arg), and branch with link to function pointer.
               We subtract a small frame (16 bytes) to be conservative / keep alignment. */
            asm volatile(
                "mov sp, %0\n"
                "sub sp, sp, #0x10\n"
                "mov x0, %1\n"
                "blr %2\n"
                :
                : "r"(stack), "r"(arg), "r"(func)
                : "x0", "sp", "memory");
			#elif defined(__arm__)
			// ARM32 implementation
			#ifdef __thumb__
            asm volatile(
				"mov sp, %0;"
                "sub sp, sp, #8;"
                "bic sp, sp, #7;"
                "push {r4-r7, lr};"
                "mov r0, %1;"
                "blx %2;"
                "pop {r4-r7, pc};"
                :
                : "r"(stack), "r"(arg), "r"(func)
                : "r0", "r1", "r2", "r3", "sp", "memory");
			#else
            asm volatile(
                "mov sp, %0;"
                "sub sp, sp, #8;"
                "bic sp, sp, #7;"
                "stmfd sp!, {r4-r7, lr};"
                "mov r0, %1;"
                "mov lr, pc;"
                "bx %2;"
                "ldmfd sp!, {r4-r7, pc}^;"
                :
                : "r"(stack), "r"(arg), "r"(func)
                : "r0", "r1", "r2", "r3", "lr", "memory");
			#endif		  
			#else
				#error "Unsupported architecture"		
			#endif		
		#elif defined(_MSC_VER)
			#if defined(_WIN64)
            __asm {
                mov rsp, stack
                sub rsp, 0x20
                mov rcx, arg
                call func
            }
			#elif defined(_M_IX86)
            __asm {
                mov esp, stack
                sub esp, 8
                push arg
                call func
            }
			#elif defined(_M_ARM64) || defined(_M_ARM)
				#error "MSVC inline assembly on ARM/ARM64 is not supported in this source. Build with clang/gcc or provide an assembly helper."
			#else
				#error "Unsupported architecture"			
			#endif
		#else
			#error "Unsupported compiler"
		#endif
        }
    }
    
    //-----------------------------------------------------------------------//
    void coro_yield(void) {
        if (!g_cur_co) {
			g_cur_co = &g_main_co;
			longjmp(g_main_co.ctx, 1);
			return;
		}
		/*
		* WARNING: setjmp does not save floating-point/vector registers.
		* If this coroutine uses FPU or SIMD, the state may be lost after yield.
		*/
        int ret = setjmp(g_cur_co->ctx);
        if (ret == 0) {
            coro_scheduler_t sched = g_scheduler ? g_scheduler : default_scheduler;
			struct coro* next = sched();
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
    void coro_destroy(struct coro* __coro__) {
        if (!__coro__) {
			g_co_errno = CO_ERR_INVAL;
			g_co_errmsg = "attempt to destroy NULL coroutine";
			return;
		}
		
		if (__coro__ == &g_main_co)
			return;
		
        while (__coro__->status == co_running || __coro__->status == co_suspend)
            coro_yield();
    
        struct coro* pre = NULL;
        struct coro* cur = g_co_list;
        while (cur != NULL) {
            if (cur == __coro__) {
                if (pre)
                    pre->next = cur->next;
                else
                    g_co_list = cur->next;
                cur->next = NULL;
                if (cur->stack) {
					size_t total_size = STACK_SIZE + get_page_size();
					stack_free(cur->stack, total_size);
				}
                free(cur);
                cur = NULL;
				__coro__ = NULL;
                g_co_cnt--;
                break;
            }
            pre = cur;
            cur = cur->next;
        }
        
        if (__coro__) {
            if (__coro__->stack != NULL) {
				size_t total_size = STACK_SIZE + get_page_size();
				stack_free(__coro__->stack, total_size);
			}
            free(__coro__);
        }
    }


    __CExternEnd__
__END__
