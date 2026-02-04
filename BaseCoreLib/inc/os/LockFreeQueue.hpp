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

template<typename ElemType>
class QueueCAS {
	static_assert(std::is_nothrow_move_constructible_v<ElemType>,
                  "ElemType must be nothrow move constructible");
    static_assert(std::is_nothrow_destructible_v<ElemType>,
                  "ElemType must be nothrow destructible");
public:
    explicit QueueCAS(size_t poolSize = 1024);
	// the caller must ensure there is no concurrency!
	// clean up leftover elements, only safe when it is certain there is no concurrency
    ~QueueCAS(void);

    bool enqueue(ElemType elem) noexcept;
    bool dequeue(ElemType& result) noexcept; 
    bool dequeue_once(ElemType& result) noexcept;
    bool try_dequeue(ElemType& result, int max_attempts = 16) noexcept;
    void dump(size_t max_elements = 1000000); // Not thread-safe, for single-threaded debugging only
    
    size_t size_approx() const noexcept {
        return approx_size.load(std::memory_order_relaxed);
    }

private:
    struct alignas(64) Node {
        union {
            ElemType elem;
        };
        std::atomic<bool> elem_initialized;
        std::atomic<Node*> next;
        std::atomic<uint64_t> version; 

        Node(void) noexcept : elem_initialized(false), next(nullptr), version(0) {}
        
        template<typename... Args>
        explicit Node(Args&&... args) noexcept 
            : elem_initialized(true), 
            next(nullptr), 
            version(0) {
            new (&elem) ElemType(std::forward<Args>(args)...);
        }
        
        ~Node() {
            if (elem_initialized.load(std::memory_order_relaxed)) {
                elem.~ElemType();
            }
        }
		
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
    };
	
	static_assert(
        sizeof(Node) % 64 == 0,
        "Node size must be multiple of 64 bytes to prevent false sharing. "
        "If failed: 1) Reduce ElemType size; 2) Add padding field to Node; "
        "3) Remove this assert if memory pool allocation pattern avoids adjacency."
    );

    class MemoryPool {
    public:
        explicit MemoryPool(size_t init_size = 1024)
        : max_pool_size((init_size > 0 ? init_size : 1024) * 64) {
			if (!expand(init_size > 0 ? init_size : 1024)) {
				throw std::bad_alloc();
		}

        ~MemoryPool(void) {
            std::lock_guard<std::mutex> lock(mtx);
            pool.clear();
            free_list.store(nullptr, std::memory_order_relaxed);
        }

        // Non-copyable, non-movable
        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;
        MemoryPool(MemoryPool&&) = delete;
        MemoryPool& operator=(MemoryPool&&) = delete;

        template<typename... Args>
        Node* allocate(Args&&... args) noexcept {
            Node* node = pop_free();
            if (!node) {
                std::lock_guard<std::mutex> lock(mtx);
                node = pop_free();
                if (!node) {
                    if (pool.size() >= max_pool_size)
                        return nullptr;
                    if (!expand(std::min(pool.size() * 2, max_pool_size))) {
                        return nullptr;
                    }
                }
                node = pop_free();
            }
            if (node) {
                if (node->elem_initialized.load(std::memory_order_relaxed)) {
                    node->elem.~ElemType();
                }
                new (&node->elem) ElemType(std::forward<Args>(args)...);
                node->elem_initialized.store(true, std::memory_order_relaxed);
                node->next.store(nullptr, std::memory_order_relaxed);
            }
            return node;
        }

        void deallocate(Node* node) noexcept {
            if (!node)
                return;
                
            node->version.fetch_add(1, std::memory_order_release);
            if (node->elem_initialized.exchange(false, std::memory_order_relaxed)) {
                node->elem.~ElemType();
            }
            
            push_free(node);
        }

    private:
        std::vector<std::unique_ptr<Node>> pool;
        alignas(64) std::atomic<Node*> free_list{ nullptr }; 
        alignas(64) std::mutex mtx;
        size_t max_pool_size;

        bool expand(size_t new_size) noexcept {
            if (new_size <= pool.size()) 
                return true;
            try {
                size_t old_size = pool.size();
                pool.resize(new_size);
                for (size_t i = old_size; i < new_size; ++i) {
                    pool[i] = std::make_unique<Node>();
                    push_free(pool[i].get());
                }
                return true;
            } catch (...) {
                if (new_size > pool.size())
                    pool.resize(pool.size());
                return false;
            }
        }

        void push_free(Node* node) noexcept {
            Node* old_head = free_list.load(std::memory_order_relaxed);
            do {
                node->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list.compare_exchange_weak(old_head, node, std::memory_order_release, std::memory_order_relaxed));
        }

        Node* pop_free(void) noexcept {
            Node* old_head = free_list.load(std::memory_order_acquire);
            Node* new_head;
            do {
                if (!old_head) return nullptr;
                new_head = old_head->next.load(std::memory_order_relaxed);
            } while (!free_list.compare_exchange_weak(old_head, new_head, std::memory_order_acquire, std::memory_order_relaxed));
            return old_head;
        }
    };

private:
    alignas(64) std::atomic<Node*> head;
    alignas(64) std::atomic<Node*> tail;
    MemoryPool pool;
    alignas(64) std::atomic<size_t> approx_size{0};

public:
    QueueCAS(const QueueCAS&) = delete;
    QueueCAS& operator=(const QueueCAS&) = delete;
};

template<typename ElemType>
QueueCAS<ElemType>::QueueCAS(size_t poolSize/* = 1024*/)
	: pool(poolSize) {
    Node* dummy = pool.allocate(); 
    if (!dummy)
        throw std::bad_alloc();
        
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

template<typename ElemType>
QueueCAS<ElemType>::~QueueCAS(void) {
    ElemType dummy;
    while (dequeue_once(dummy)) { }
    
    Node* dummy_node = head.load(std::memory_order_relaxed);
    if (dummy_node) {
        pool.deallocate(dummy_node);
    }
}

template<typename ElemType>
bool QueueCAS<ElemType>::enqueue(ElemType elem) noexcept {
    Node* new_node = pool.allocate(std::move(elem));
    if (!new_node) {
        return false;
    }

    while (true) {
        Node* old_tail = tail.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_acquire);

        Node* next = old_tail->next.load(std::memory_order_relaxed);
        uint64_t old_version = old_tail->version.load(std::memory_order_relaxed);
		
        if (tail.load(std::memory_order_relaxed) != old_tail ||
            old_tail->version.load(std::memory_order_relaxed) != old_version) {
            continue;
        }

		if (next == nullptr) {
            if (old_tail->next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release, std::memory_order_relaxed);
                approx_size.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
        } else {
            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
        }
    }
}

template<typename ElemType>
bool QueueCAS<ElemType>::dequeue(ElemType& result) noexcept {
    // Try a reasonable number of times, balancing success rate and CPU usage
    return try_dequeue(result, 32);
}

template<typename ElemType>
bool QueueCAS<ElemType>::dequeue_once(ElemType& result) noexcept {
    // Only one
    return try_dequeue(result, 1);
}

template<typename ElemType>
bool QueueCAS<ElemType>::try_dequeue(ElemType& result, int max_attempts) noexcept {
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        Node* old_head = head.load(std::memory_order_acquire);
		std::atomic_thread_fence(std::memory_order_acquire);
		
        Node* next = old_head->next.load(std::memory_order_relaxed);
        uint64_t old_version = old_head->version.load(std::memory_order_relaxed);
        Node* old_tail = tail.load(std::memory_order_relaxed);
		
        if (head.load(std::memory_order_relaxed) != old_head ||
            old_head->version.load(std::memory_order_relaxed) != old_version) {
            continue;
        }

        if (old_head == old_tail) {
            if (next == nullptr) {
                return false; 
            }

            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
            continue;
        }

        if (!next) {
            continue;
        }
        
        if (!next->elem_initialized.load(std::memory_order_acquire)) {
            continue;
        }
        
		uint64_t next_version_before = next->version.load(std::memory_order_acquire);
        
        if (head.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_relaxed)) {
            // check the version number of the next node again to prevent the extremely rare ABA problem.
            uint64_t next_version_after = next->version.load(std::memory_order_acquire);
            if (next_version_after != next_version_before) {
                // version number changed, indicating the node might have been recycled and reused
				// in this case, the data may have been moved, but we need to ensure safety
				// since we haven't moved the data yet, just roll back the head and retry
				Node* temp = next;
                head.compare_exchange_weak(temp, old_head, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
            
            // move data safely
            result = std::move(next->elem);
            pool.deallocate(old_head);
            
            approx_size.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
	
    return false;
}

template<typename ElemType>
void QueueCAS<ElemType>::dump(size_t max_elements/* = 1000000*/) {
	Node* current = head.load(std::memory_order_relaxed);
    if (!current)
        return;
    
    current = current->next.load(std::memory_order_relaxed);

    std::cout << "Queue elements (approx size: " << size_approx() << "): ";

    if (!current) {
        std::cout << "Empty" << std::endl;
        return;
    }
    
    size_t count = 0;
	size_t displayed = 0;
    bool first = true;
    
    while (current && count < max_elements) {
        if (current->elem_initialized.load(std::memory_order_relaxed)) {
            if (!first) {
                std::cout << " ";
            }
            std::cout << current->elem;
            first = false;
			displayed++;
        }
        count++;
        current = current->next.load(std::memory_order_relaxed);
    }
    
    if (current) {
        std::cout << " ...";
        std::cout << " (showing " << displayed << " of " << size_approx() << " elements)";
    } else {
        std::cout << " (total " << displayed << " elements)";
    }
	
    std::cout << std::endl;
}

#endif // __LockFreeQueue_h__
