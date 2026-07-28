/*****************************************************************************
* FileName    : LockFreeQueue.hpp
* Description : Free-lock queues definition, implemented in c++11
* Author      : Joe.Bi
* Date        : 2024-04
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __LockFreeQueue_h__
#define __LockFreeQueue_h__
#include "../base/Macro.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>
#include <thread>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include <cstddef>
#include <cassert>
#include <stdexcept>
#include <ostream>
#include <unordered_map>

__BEGIN__

	//-----------------------------------------------------------------------//
	#ifndef CACHE_LINE_SIZE
		// x86/x64 (both 32 and 64 bits) 64 bytes
		#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
			#define CACHE_LINE_SIZE 64
		// ARM 32/64: overwhelmingly 64 bytes, some old 32-bit are 32.
		// We default to 64 to be safe; if targeting a known 32 byte platform,
		// the user can override via -DCACHE_LINE_SIZE=32.
		#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
			#define CACHE_LINE_SIZE 64
		// POWER / Cell / some servers 128 bytes
		#elif defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__)
			#define CACHE_LINE_SIZE 128
		// Generic fallback (reasonable guess on most modern architectures)
		#else
			#define CACHE_LINE_SIZE 64
		#endif
	#endif

	//-----------------------------------------------------------------------//
	#if defined(_MSC_VER)
		#include <intrin.h>
		inline void cpu_relax() { YieldProcessor(); }
	#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
		#include <immintrin.h>
		inline void cpu_relax() { _mm_pause(); }
	#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
		inline void cpu_relax() { __asm__ volatile("yield" ::: "memory"); }
	#else
		// General fallback: just a compiler barrier to prevent the loop from being optimized away,
		// but it doesn't guarantee the CPU will save power
		inline void cpu_relax() { __asm__ volatile("" ::: "memory"); }
	#endif
	
	//-----------------------------------------------------------------------//
	// Per-queue hazard pointer manager
	// Thread safe registration / unregistration of HP slots.
	// Each thread obtains exactly one slot per unique MaxSlots value.
	// Slot is automatically released when the thread exits (RAII guard).
	//-----------------------------------------------------------------------//
	class HpManager 
	{
	public:
		HpManager(size_t MaxSlots = 1024) 
		: hp_array(new std::atomic<void*>[MaxSlots]),
		  hp_free_stack(new std::atomic<uint32_t>[MaxSlots]),
		  hp_free_top(0),
		  max_slots(MaxSlots)
		{
			for (size_t i = 0; i < max_slots; ++i)
			{
				hp_array[i].store(nullptr, std::memory_order_relaxed);
				hp_free_stack[i].store(static_cast<uint32_t>(i), std::memory_order_relaxed);
			}
			hp_free_top.store(static_cast<uint32_t>(max_slots), std::memory_order_relaxed);
		}
		
		~HpManager() = default;
		HpManager(const HpManager&) = delete;
		HpManager& operator=(const HpManager&) = delete;

		// Must be called from the thread that owns the QueueCAS before it is destroyed.
		// Releases the current thread's HP slot and removes its automatic cleanup guard.
		void cleanup_current_thread() noexcept 
		{
			uint32_t& slot_ref = get_thread_slot();
			if (slot_ref == UINT32_MAX) 
				return;
		
			auto& guards = get_guards();
			auto it = guards.find(this);
			if (it != guards.end())
				guards.erase(it);

			slot_ref = UINT32_MAX;
		}
	
		// Returns false if all slots are exhausted.
		bool try_register_thread() noexcept 
		{
			uint32_t& slot = get_thread_slot();
			if (slot != UINT32_MAX) return true;
	
			// Pop an index from the free stack (lock-free)
			uint32_t old_top = hp_free_top.load(std::memory_order_acquire);
			while (old_top > 0) 
			{
				if (hp_free_top.compare_exchange_weak(old_top, old_top - 1, std::memory_order_acquire, std::memory_order_relaxed))
				{
					slot = hp_free_stack[old_top - 1].load(std::memory_order_acquire);
					
					// RAII guard for automatic cleanup at thread exit
					auto& guards = get_guards();
					if (guards.find(this) == guards.end())
						guards.emplace(this, ThreadGuard{this, slot});

					return true;
				}
			}
			return false;
		}
	
		void unregister_thread(uint32_t slot) noexcept 
		{
			if (slot == UINT32_MAX) 
				return;
	
			hp_array[slot].store(nullptr, std::memory_order_release);
	
			uint32_t old_top = hp_free_top.load(std::memory_order_acquire);
			hp_free_stack[old_top].store(slot, std::memory_order_release);
			do{} while (!hp_free_top.compare_exchange_weak(old_top, old_top + 1, std::memory_order_release, std::memory_order_relaxed));
		}
	
		void protect(void* p) noexcept
		{
			const uint32_t slot = get_thread_slot();
			if (slot != UINT32_MAX)
				hp_array[slot].store(p, std::memory_order_release);
		}
	
		void clear_protect() noexcept 
		{
			const uint32_t slot = get_thread_slot();
			if (slot != UINT32_MAX)
				hp_array[slot].store(nullptr, std::memory_order_release);
		}
	
		// scans all slots, returns true if 'p' is still protected by any thread.
		bool is_protected(void* p) const noexcept 
		{
			for (size_t i = 0; i < max_slots; ++i)
			{
				if (hp_array[i].load(std::memory_order_acquire) == p)
					return true;
			}
			return false;
		}
	
	private:
		// Nested type for automatic cleanup at thread exit
		struct ThreadGuard 
		{
			HpManager* owner;
			uint32_t   slot;
			ThreadGuard(HpManager* hp, uint32_t s) : owner(hp), slot(s) { }
			~ThreadGuard() 
			{
				owner->unregister_thread(slot);
			}
		};

		static std::unordered_map<const HpManager*, ThreadGuard>& get_guards() 
		{
			static threadlocal std::unordered_map<const HpManager*, ThreadGuard> guards;
			return guards;
		}

		static uint32_t& get_thread_slot_for(const HpManager* mgr) 
		{
			// Make sure each thread only corresponds to the current QueueCAS
			static threadlocal std::unordered_map<const HpManager*, uint32_t> slots;
			auto it = slots.find(mgr);
			if (it == slots.end())
			{
				auto res = slots.emplace(mgr, UINT32_MAX);
				return res.first->second;
			}
			return it->second;
		}
	
		uint32_t& get_thread_slot() 
		{
			return get_thread_slot_for(this);
		}
	
		const std::unique_ptr<std::atomic<void*>[]>      hp_array;
		const std::unique_ptr<std::atomic<uint32_t>[]>   hp_free_stack;
		alignas(CACHE_LINE_SIZE) std::atomic<uint32_t>   hp_free_top;
		size_t 											 max_slots;
	};
	
	//-----------------------------------------------------------------------//
	// Tagged pointer: 16-byte aligned, contains pointer + 64-bit version.
	// Used to eliminate ABA by making CAS compare both pointer and version atomically.
	//-----------------------------------------------------------------------//
	template<typename NodeType>
	struct alignas(sizeof(void*) >= 8 ? 16 : 8) TaggedPtr
	{
		NodeType* ptr;
		typename std::conditional<(sizeof(void*) >= 8), std::uint64_t, std::uint32_t>::type version;
		
		TaggedPtr() noexcept : ptr(nullptr), version(0) {}
		TaggedPtr(NodeType* p, decltype(version) v) noexcept : ptr(p), version(v) {}
		
		bool operator==(const TaggedPtr& rhs) const noexcept
		{
			return ptr == rhs.ptr && version == rhs.version;
		}
		bool operator!=(const TaggedPtr& rhs) const noexcept 
		{
			return !(*this == rhs);
		}
		
		static_assert(std::is_trivially_copyable<TaggedPtr>::value, 
						"TaggedPtr must be trivially copyable for atomic usage");
	};
	
	//-----------------------------------------------------------------------//
	// Lock-free queue (multi-producer, multi-consumer)
	//-----------------------------------------------------------------------//
	template<typename ElemType>
	class QueueCAS 
	{
    static_assert(std::is_nothrow_move_constructible<ElemType>::value,
					"ElemType must be nothrow move constructible");
    static_assert(std::is_nothrow_destructible<ElemType>::value,
					"ElemType must be nothrow destructible");
    static_assert(std::is_nothrow_move_assignable<ElemType>::value,
                  "ElemType must be nothrow move assignable");
	public:
		explicit QueueCAS(size_t poolSize = 1024);
		// Destructor waits for all active operations to finish and for all registered threads
		// to exit. To avoid deadlock, join all threads before destruction.
		~QueueCAS(void);
	
		bool enqueue(ElemType elem) noexcept;
		bool dequeue(ElemType& result) noexcept; 
		bool dequeue_once(ElemType& result) noexcept;
		bool try_dequeue(ElemType& result, int max_attempts = 16) noexcept;
		void dump(std::ostream& os, size_t max_elements = 1000000); // For single-threaded or stationary debugging only
		
		size_t size_approx() const noexcept 
		{
			return approx_size.load(std::memory_order_relaxed);
		}
	
	private:
		//-------------------------------------------------------------------//
		// Node: holds element and next pointer (with version)
		//-------------------------------------------------------------------//
		struct alignas(CACHE_LINE_SIZE) Node 
		{
			typename std::aligned_storage<sizeof(ElemType), alignof(ElemType)>::type storage;
			std::atomic<bool> elem_initialized;
			std::atomic<TaggedPtr<Node>> next;
	
			ElemType& elem() { return *static_cast<ElemType*>(static_cast<void*>(&storage)); }
			const ElemType& elem() const { return *static_cast<const ElemType*>(static_cast<const void*>(&storage)); }

			Node(void) noexcept : elem_initialized(false), next(TaggedPtr<Node>(nullptr, 0)) {}
			
			template<typename... Args>
			explicit Node(Args&&... args) noexcept 
			: elem_initialized(true), 
			next(TaggedPtr<Node>(nullptr, 0))
			{
				new (&storage) ElemType(std::forward<Args>(args)...);
			}
			
			template<typename... Args>
			void initElem(Args&&... args) noexcept(noexcept(::new (&storage) ElemType(std::forward<Args>(args)...))) 
			{
				new (&storage) ElemType(std::forward<Args>(args)...);
				elem_initialized.store(true, std::memory_order_release);
			}
			
			~Node()
			{
				if (elem_initialized.load(std::memory_order_relaxed))
					elem().~ElemType();
			}
			
			Node(const Node&) = delete;
			Node& operator=(const Node&) = delete;
		};
		
		static_assert(alignof(Node) == CACHE_LINE_SIZE, 
						"Node must be aligned to CACHE_LINE_SIZE bytes to prevent false sharing");
	
		//-------------------------------------------------------------------//
		// Memory pool: reuses nodes to avoid frequent allocation.
		// free_list uses tagged pointer for ABA safety.
		//-------------------------------------------------------------------//
		class MemoryPool 
		{
		public:
			explicit MemoryPool(size_t init_size = 1024)
			: max_pool_size(init_size > 0 ? init_size : 1024)
			{
				if (!expand(max_pool_size))		
					throw std::bad_alloc();
			}
	
			~MemoryPool(void) 
			{
				std::lock_guard<std::mutex> lock(mtx);
				pool.clear();
				free_list.store(TaggedPtr<Node>(nullptr, 0), std::memory_order_relaxed);
			}
	
			// Non-copyable, non-movable
			MemoryPool(const MemoryPool&) = delete;
			MemoryPool& operator=(const MemoryPool&) = delete;
			MemoryPool(MemoryPool&&) = delete;
			MemoryPool& operator=(MemoryPool&&) = delete;
	
			template<typename... Args>
			TaggedPtr<Node> allocate(Args&&... args) noexcept 
			{
				TaggedPtr<Node> tp = obtain_node();
				if (!tp.ptr) 
					return tp;
					
				Node* node = tp.ptr;
				if (node->elem_initialized.load(std::memory_order_acquire))
					node->elem().~ElemType();
        
				node->initElem(std::forward<Args>(args)...);
				node->next.store(TaggedPtr<Node>(nullptr, tp.version), std::memory_order_relaxed);
				
				return tp;
			}
			
			TaggedPtr<Node> allocate(void) noexcept
			{
				TaggedPtr<Node> tp = obtain_node();
				if (!tp.ptr) 
					return tp;
					
				Node* node = tp.ptr;
				if (node->elem_initialized.load(std::memory_order_acquire)) 
				{
					node->elem().~ElemType();
					node->elem_initialized.store(false, std::memory_order_relaxed);
				}
				
				node->next.store(TaggedPtr<Node>(nullptr, tp.version), std::memory_order_relaxed);
				
				return tp;
			}
	
			void deallocate(TaggedPtr<Node> node) noexcept 
			{
				if (!node.ptr) 
					return;
				
				if (node.ptr->elem_initialized.exchange(false, std::memory_order_relaxed))
					node.ptr->elem().~ElemType();
				
				push_free(node);
			}
	
		private:
			std::vector<std::unique_ptr<Node>> pool;
			alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> free_list{ TaggedPtr<Node>(nullptr, 0) }; 
			alignas(CACHE_LINE_SIZE) std::mutex mtx;
			size_t max_pool_size;
	
			bool expand(size_t new_size) noexcept 
			{
				if (new_size <= pool.size()) 
					return true;
				try 
				{
					size_t old_size = pool.size();
					
					std::vector<std::unique_ptr<Node>> new_nodes(new_size - old_size);
					for (auto& node : new_nodes)
						node = std::unique_ptr<Node>(new Node()); // C++11 compatible
	
					pool.resize(new_size);
					for (size_t i = 0; i < new_nodes.size(); ++i) 
					{
						pool[old_size + i] = std::move(new_nodes[i]);
						push_free(TaggedPtr<Node>(pool[old_size + i].get(), 0), true);
					}
	
					return true;
				} 
				catch (...) 
				{
					return false;
				}
			}
			
			void push_free(TaggedPtr<Node> node, bool newNode = false) noexcept 
			{
				TaggedPtr<Node> old_head = free_list.load(std::memory_order_relaxed);
				TaggedPtr<Node> new_head;
				do {
					new_head.ptr = node.ptr;
					new_head.version = newNode ? 0 : node.version + 1;
					new_head.ptr->next.store(old_head, std::memory_order_relaxed);
				} while (!free_list.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));
			}

	
			TaggedPtr<Node> pop_free(void) noexcept
			{
				TaggedPtr<Node> old_head = free_list.load(std::memory_order_acquire);
				TaggedPtr<Node> new_head;
				do {
					if (!old_head.ptr) 
						return TaggedPtr<Node>(nullptr, 0);
						
					new_head = old_head.ptr->next.load(std::memory_order_relaxed);
				} while (!free_list.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed));
				
				return old_head;
			}
			
			TaggedPtr<Node> obtain_node() noexcept 
			{
				TaggedPtr<Node> tp = pop_free();
				if (tp.ptr) 
					return tp;
					
				std::lock_guard<std::mutex> lock(mtx);
				while ((tp = pop_free()).ptr == nullptr)
				{
					if (pool.size() >= max_pool_size)
						return TaggedPtr<Node>(nullptr, 0);
					if (!expand(std::min(pool.size() * 2, max_pool_size)))
						return TaggedPtr<Node>(nullptr, 0);
				}
				return tp;
			}
		};
		
		//-------------------------------------------------------------------//
		// Hazard pointer helpers functions - forward to the manager with our MaxHp.
		void protect(Node* p) noexcept 
		{
			hp_manager.protect(static_cast<void*>(p));
		}
		
		void clear_protect() noexcept
		{
			hp_manager.clear_protect();
		}
		
		bool is_protected(Node* p) noexcept 
		{
			return hp_manager.is_protected(static_cast<void*>(p));
		}
		
		//-------------------------------------------------------------------//
		// Retire / reclaim list (lock-free stack)
		void retire(TaggedPtr<Node> node) noexcept
		{
			if (!node.ptr) 
				return;
			TaggedPtr<Node> old_head = retire_head.load(std::memory_order_relaxed);
			TaggedPtr<Node> new_head;
			do 
			{
				new_head.ptr = node.ptr;
				new_head.version = node.version + 1;
				node.ptr->next.store(old_head, std::memory_order_relaxed);
			} while (!retire_head.compare_exchange_weak(old_head, new_head, std::memory_order_release, std::memory_order_relaxed));
		}
	
		void reclaim_batch(size_t batch_size = 16) noexcept
		{
			for (size_t i = 0; i < batch_size; ++i) 
			{
				TaggedPtr<Node> old_head = retire_head.load(std::memory_order_acquire);
				if (!old_head.ptr) 
					break;
				TaggedPtr<Node> next = old_head.ptr->next.load(std::memory_order_relaxed);
				if (!retire_head.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_relaxed))
					continue;
				if (!is_protected(old_head.ptr))
					pool.deallocate(old_head);
				else
					retire(old_head);
			}
		}
	
		void force_reclaim_all() noexcept 
		{
			TaggedPtr<Node> cur = retire_head.load(std::memory_order_relaxed);
			while (cur.ptr) 
			{
				TaggedPtr<Node> next = cur.ptr->next.load(std::memory_order_relaxed);
				pool.deallocate(cur);
				cur = next;
			}
			
			retire_head.store(TaggedPtr<Node>(nullptr, 0), std::memory_order_relaxed);
		}
	
		//-------------------------------------------------------------------//
		// Enter / leave critical section
		bool enter() noexcept
		{
			active_users.fetch_add(1, std::memory_order_release);
			if (shutdown_flag.load(std::memory_order_acquire)) 
			{
				active_users.fetch_sub(1, std::memory_order_release);
				return false;
			}
			
			if (!hp_manager.try_register_thread()) 
			{
				active_users.fetch_sub(1, std::memory_order_release);
				return false;
			}
			
			return true;
		}
	
		void leave() noexcept
		{
			active_users.fetch_sub(1, std::memory_order_release);
		}
	
	private:
		alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> head;
		alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> tail;
		MemoryPool pool;
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> approx_size;
		HpManager hp_manager;
		// retire list
		alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> retire_head;
		// all thread shutdown flags and active user count
		alignas(CACHE_LINE_SIZE) std::atomic<bool> shutdown_flag;
		alignas(CACHE_LINE_SIZE) std::atomic<int> active_users;
	public:
		QueueCAS(const QueueCAS&) = delete;
		QueueCAS& operator=(const QueueCAS&) = delete;
	};
	
	//-----------------------------------------------------------------------//
template<typename ElemType>
	QueueCAS<ElemType>::QueueCAS(size_t poolSize/* = 1024*/)
    : head(TaggedPtr<Node>(nullptr, 0)),
      tail(TaggedPtr<Node>(nullptr, 0)),
      pool(poolSize),
	  hp_manager(),
      approx_size(0),
	  retire_head(TaggedPtr<Node>(nullptr, 0)),
      shutdown_flag(false),
      active_users(0)
	{  
		if (!head.is_lock_free())
			throw std::runtime_error("atomic<TaggedPtr<Node>> is not lock-free on this platform");

		TaggedPtr<Node> dummy = pool.allocate(); 
		if (!dummy.ptr)
			throw std::bad_alloc();
			
		head.store(dummy, std::memory_order_relaxed);
		tail.store(dummy, std::memory_order_relaxed);
	}
	
	template<typename ElemType>
	QueueCAS<ElemType>::~QueueCAS(void) 
	{
		shutdown_flag.store(true, std::memory_order_release);
		while (active_users.load(std::memory_order_acquire) > 0)
			std::this_thread::yield();
		
		assert(active_users.load() == 0);

		hp_manager.cleanup_current_thread();

		TaggedPtr<Node> cur = head.load(std::memory_order_relaxed);
		while (cur.ptr) 
		{
			TaggedPtr<Node> next = cur.ptr->next.load(std::memory_order_relaxed);
			pool.deallocate(cur);
			cur = next;
		}
		
		force_reclaim_all();
	}
	
	template<typename ElemType>
	bool QueueCAS<ElemType>::enqueue(ElemType elem) noexcept 
	{
		if (!enter()) 
			return false;
		
		TaggedPtr<Node> new_node = pool.allocate(std::move(elem));
		if (!new_node.ptr)
		{
			leave();
			return false;
		}
		
		while (true) 
		{
        TaggedPtr<Node> old_tail = tail.load(std::memory_order_acquire);
        protect(old_tail.ptr);
        if (old_tail != tail.load(std::memory_order_acquire)) 
		{
            clear_protect();
            continue;
        }

        TaggedPtr<Node> next = old_tail.ptr->next.load(std::memory_order_acquire);
        if (!next.ptr)
		{
            if (old_tail.ptr->next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_relaxed)) 
			{
                tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release, std::memory_order_relaxed);
                approx_size.fetch_add(1, std::memory_order_relaxed);
                clear_protect();
                leave();
                return true;
            }
        } 
		else 
            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
			
        clear_protect();
	}
	
	template<typename ElemType>
	bool QueueCAS<ElemType>::dequeue(ElemType& result) noexcept 
    {
		if (!enter())
			return false;
		
		int backoff = 1;
		constexpr int max_backoff = 1024;
		
		while (true) 
		{
			if (try_dequeue(result, 16))
			{
				reclaim_batch(16);
				leave();
				return true;
			}
	
			TaggedPtr<Node> h = head.load(std::memory_order_acquire);
			protect(h.ptr);
			if (h != head.load(std::memory_order_acquire))
			{
				clear_protect();
				continue;
			}
			TaggedPtr<Node> t = tail.load(std::memory_order_acquire);
			if (h.ptr == t.ptr && h.ptr->next.load(std::memory_order_relaxed).ptr == nullptr)
			{
				clear_protect();
				reclaim_batch(16);
				leave();
				return false;
			}
			clear_protect();
	
			if (backoff < max_backoff) 
			{
				for (int i = 0; i < backoff; ++i)
					cpu_relax();
				if (backoff > 16)
					std::this_thread::yield();
				backoff *= 2;
			} 
			else
				std::this_thread::yield();
		}
	}
	
	template<typename ElemType>
	bool QueueCAS<ElemType>::dequeue_once(ElemType& result) noexcept 
    {
		if (!enter()) 
			return false;
			
		bool ok = try_dequeue(result, 1);
		reclaim_batch(16);
		leave();	
		
		return ok;
	}
	
	template<typename ElemType>
	bool QueueCAS<ElemType>::try_dequeue(ElemType& result, int max_attempts) noexcept 
	{
		for (int attempt = 0; attempt < max_attempts; ++attempt) 
		{
			TaggedPtr<Node> old_head = head.load(std::memory_order_acquire);
			protect(old_head.ptr);
			if (old_head != head.load(std::memory_order_acquire)) 
			{
				clear_protect();
				continue;
			}
	
			TaggedPtr<Node> next = old_head.ptr->next.load(std::memory_order_acquire);
			if (!next.ptr) 
			{
				clear_protect();
				return false;
			}
			
			protect(next.ptr);
			
			if (old_head != head.load(std::memory_order_acquire)) 
			{
				clear_protect();
				continue;
			}
	
			TaggedPtr<Node> old_tail = tail.load(std::memory_order_acquire);
			if (old_head.ptr == old_tail.ptr)
			{
				tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
				clear_protect();
				continue;
			}
	
			if (!next.ptr->elem_initialized.load(std::memory_order_acquire))
			{
				clear_protect();
				continue;
			}
	
			if (head.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_relaxed)) 
			{
				ElemType tmp(std::move(next.ptr->elem()));
				next.ptr->elem_initialized.store(false, std::memory_order_release);
				std::swap(result, tmp);
				approx_size.fetch_sub(1, std::memory_order_relaxed);
				clear_protect();
				retire(old_head);
				return true;
			}
			clear_protect();
		}
		clear_protect();
		return false;
	}
	
	template<typename ElemType>
	void QueueCAS<ElemType>::dump(std::ostream& os, size_t max_elements/* = 1000000*/)
	{
		TaggedPtr<Node> curr = head.load(std::memory_order_relaxed);
		Node* node = curr.ptr ? curr.ptr->next.load(std::memory_order_relaxed).ptr : nullptr;

		os << "Queue elements (approx size: " << size_approx() << "): ";
		if (!node) 
		{
			os << "Empty" << std::endl;
			return;
		}

		size_t count = 0, displayed = 0;
		bool first = true;
		while (node && count < max_elements) 
		{
			if (node->elem_initialized.load(std::memory_order_relaxed))
			{
				if (!first) os << " ";
				os << node->elem();
				first = false;
				++displayed;
			}
			++count;
			node = node->next.load(std::memory_order_relaxed).ptr;
		}
		
		if (node)
		{
			os << " ...";
			os << " (showing " << displayed << " of " << size_approx() << " elements)";
		} 
		else 
			os << " (total " << displayed << " elements)";
		
		os << std::endl;
	}

__END__	

#endif // __LockFreeQueue_h__
