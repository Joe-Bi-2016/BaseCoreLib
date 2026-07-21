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
#include <atomic>
#include <iostream>
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

//---------------------------------------------------------------------------//
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
	// Tagged pointer: 16-byte aligned, contains pointer + 64-bit version.
	// Used to eliminate ABA by making CAS compare both pointer and version atomically.
	template<typename NodeType>
	struct alignas(sizeof(void*) >= 8 ? 16 : 8) TaggedPtr
	{
		NodeType* ptr;
		typename std::conditional<(sizeof(void*) >= 8), uint64_t, uint32_t>::type version;
		
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
	template<typename ElemType>
	class QueueCAS 
	{
    static_assert(std::is_nothrow_move_constructible<ElemType>::value,
					"ElemType must be nothrow move constructible");
    static_assert(noexcept(std::declval<ElemType&>().~ElemType()),
					"ElemType must be nothrow destructible");
	public:
		explicit QueueCAS(size_t poolSize = 1024);
		// Destructor waits for all active operations to finish and for all registered threads
		// to exit. To avoid deadlock, join all threads before destruction.
		~QueueCAS(void);
	
		bool enqueue(ElemType elem) noexcept;
		bool dequeue(ElemType& result) noexcept; 
		bool dequeue_once(ElemType& result) noexcept;
		bool try_dequeue(ElemType& result, int max_attempts = 16) noexcept;
		void dump(size_t max_elements = 1000000); // For single-threaded or stationary debugging only
		
		size_t size_approx() const noexcept 
		{
			return approx_size.load(std::memory_order_relaxed);
		}
	
	private:
		//-------------------------------------------------------------------//
		// Node: holds element and next pointer (with version)
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
		struct alignas(CACHE_LINE_SIZE) HpNode
		{
			std::atomic<Node*> ptr;
			std::atomic<HpNode*> next;
			HpNode() noexcept : ptr(nullptr), next(nullptr) {}
			~HpNode()
			{
				ptr.store(nullptr, std::memory_order_relaxed);
				next.store(nullptr, std::memory_order_relaxed);
			}
		};
	
		static HpNode*& thread_hp_node() 
		{
			static thread_local HpNode* node = nullptr;
			return node;
		}
	
		void ensure_thread_registered() 
		{
			HpNode*& my = thread_hp_node();
			if (!my)
			{
				my = new HpNode();
				{
					std::lock_guard<std::mutex> lock(hp_mutex);
					HpNode* old = hp_list_head.load(std::memory_order_relaxed);
					my->next.store(old, std::memory_order_relaxed);
					hp_list_head.store(my, std::memory_order_relaxed);
				}
				
				register_count.fetch_add(1, std::memory_order_release);
		
				struct hp_list_guard 
				{
					HpNode* n;
					QueueCAS* q;
					hp_list_guard(HpNode* node, QueueCAS* queue) : n(node), q(queue) {}
					~hp_list_guard()
					{ 
						q->flush_local_retire(); // thread exit, flush it's local retire list to global
						q->unregister_thread(n);
						delete n;
						thread_hp_node() = nullptr;
					}
				};
				
				static thread_local hp_list_guard guard(my, this);
			}
		}
		
		void unregister_thread(HpNode* node) 
		{
			{
				std::lock_guard<std::mutex> lock(hp_mutex);
				HpNode* prev = nullptr;
				HpNode* curr = hp_list_head.load(std::memory_order_relaxed);
				while (curr) 
				{
					if (curr == node) 
					{
						if (prev)
							prev->next.store(curr->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
						else
							hp_list_head.store(curr->next.load(std::memory_order_relaxed), std::memory_order_relaxed);
						break;
					}
					prev = curr;
					curr = curr->next.load(std::memory_order_relaxed);
				}
			}
			
			register_count.fetch_sub(1, std::memory_order_release);
		}

	
		void protect(Node* p)
		{
			HpNode* my = thread_hp_node();
			if (my) 
				my->ptr.store(p, std::memory_order_release);
		}
	
		void clear_protect() 
		{
			HpNode* my = thread_hp_node();
			if (my) 
				my->ptr.store(nullptr, std::memory_order_release);
		}
	
		bool is_protected(Node* p) 
		{
			std::lock_guard<std::mutex> lock(hp_mutex);
			HpNode* n = hp_list_head.load(std::memory_order_acquire);
			while (n)
			{
				if (n->ptr.load(std::memory_order_acquire) == p)
					return true;
				n = n->next.load(std::memory_order_acquire);
			}
			return false;
		}
	
		//-------------------------------------------------------------------//
		static std::vector<TaggedPtr<Node>>& get_local_retire_list()
		{
			static thread_local std::vector<TaggedPtr<Node>> list;
			return list;
		}

		void flush_local_retire()
		{
			auto& local = get_local_retire_list();
			if (local.empty()) 
				return;
			{
				std::lock_guard<std::mutex> lock(retire_mtx);
				retire_list.insert(retire_list.end(), local.begin(), local.end());
				local.clear();
			}

			try_reclaim();
		}
		
		void retire(TaggedPtr<Node> node) noexcept
		{
			if (!node.ptr) 
				return;
			
			auto& local = get_local_retire_list();
			local.push_back(node);
			
			if (local.size() >= 128)
				flush_local_retire();
		}
	
		void try_reclaim()
		{
			std::lock_guard<std::mutex> lock(retire_mtx);
			if (retire_list.empty()) 
				return;
				
			std::vector<TaggedPtr<Node>> remaining;
			for (auto& tp : retire_list) 
			{
				if (!is_protected(tp.ptr))
					pool.deallocate(tp);
				else
					remaining.push_back(tp);
			}
			
			retire_list.swap(remaining);
		}
		
		void force_reclaim_all()
		{
			for (auto& tp : retire_list)
				pool.deallocate(tp);
				
			retire_list.clear();
		}
		
		//-------------------------------------------------------------------//
		bool enter()
		{
			active_users.fetch_add(1, std::memory_order_release);
			if (shutdown_flag.load(std::memory_order_acquire)) 
			{
				active_users.fetch_sub(1, std::memory_order_release);
				return false;
			}
			
			ensure_thread_registered();
			
			return true;
		}
	
		void leave()
		{
			active_users.fetch_sub(1, std::memory_order_release);
		}
	
	private:
		alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> head;
		alignas(CACHE_LINE_SIZE) std::atomic<TaggedPtr<Node>> tail;
		MemoryPool pool;
		alignas(CACHE_LINE_SIZE) std::atomic<size_t> approx_size;
		// global hazard pointer list
		alignas(CACHE_LINE_SIZE) std::atomic<HpNode*> hp_list_head{nullptr}; 
		alignas(CACHE_LINE_SIZE) std::atomic<unsigned> register_count;
		alignas(CACHE_LINE_SIZE) std::mutex hp_mutex;
		// safe retire list
		std::mutex retire_mtx;
		std::vector<TaggedPtr<Node>> retire_list;
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
      approx_size(0),
	  hp_list_head(nullptr),
	  register_count(0),
      shutdown_flag(false),
      active_users(0)
	{  
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
	
		while (register_count.load(std::memory_order_acquire) > 0)
			std::this_thread::yield();
		
		HpNode* hp = hp_list_head.load(std::memory_order_relaxed);
		while (hp) 
		{
			HpNode* next = hp->next.load(std::memory_order_relaxed);
			delete hp;
			hp = next;
		}
		
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
				leave();
				try_reclaim();
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
				leave();
				try_reclaim();
				return false;
			}
			clear_protect();
	
			for (int i = 0; i < backoff; ++i)
				cpu_relax();
				
			if (backoff < max_backoff)
				backoff *= 2;
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
		leave();
		if (ok) 
			try_reclaim();
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
	void QueueCAS<ElemType>::dump(size_t max_elements/* = 1000000*/)
	{
		TaggedPtr<Node> curr = head.load(std::memory_order_relaxed);
		Node* node = curr.ptr ? curr.ptr->next.load(std::memory_order_relaxed).ptr : nullptr;

		std::cout << "Queue elements (approx size: " << size_approx() << "): ";
		if (!node) 
		{
			std::cout << "Empty" << std::endl;
			return;
		}

		size_t count = 0, displayed = 0;
		bool first = true;
		while (node && count < max_elements) 
		{
			if (node->elem_initialized.load(std::memory_order_relaxed))
			{
				if (!first) std::cout << " ";
				std::cout << node->elem();
				first = false;
				++displayed;
			}
			++count;
			node = node->next.load(std::memory_order_relaxed).ptr;
		}
		
		if (node)
		{
			std::cout << " ...";
			std::cout << " (showing " << displayed << " of " << size_approx() << " elements)";
		} 
		else 
			std::cout << " (total " << displayed << " elements)";
		
		std::cout << std::endl;
	}

__END__	

#endif // __LockFreeQueue_h__
