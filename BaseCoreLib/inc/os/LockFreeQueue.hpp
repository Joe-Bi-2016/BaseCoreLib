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
public:
    QueueCAS(void);
    ~QueueCAS(void);

    bool enqueue(ElemType elem) noexcept;
    bool dequeue(ElemType& result) noexcept; 
    bool dequeue_once(ElemType& result) noexcept;
    bool try_dequeue(ElemType& result, int max_attempts = 2) noexcept;
    void dump(void);

private:
    typedef struct alignas(64) qNode {
        union {
            ElemType elem;
        };
        bool elem_initialized;
        std::atomic<struct qNode*> next;
        std::atomic<size_t> version; 

        qNode(void) : elem_initialized(false), next(nullptr), version(0) {}
        
        template<typename... Args>
        explicit qNode(Args&&... args) : 
            elem_initialized(true), 
            next(nullptr), 
            version(0) {
            new (&elem) ElemType(std::forward<Args>(args)...);
        }
        
        ~qNode() {
            if (elem_initialized) {
                elem.~ElemType();
            }
        }
    } Node;

    class MemoryPool {
    public:
        explicit MemoryPool(size_t init_size = 1024) {
            expand(init_size);
        }

        ~MemoryPool(void) {
            std::lock_guard<std::mutex> lock(mtx);
            pool.clear();
            free_list.store(nullptr, std::memory_order_relaxed);
        }

        template<typename... Args>
        Node* allocate(Args&&... args) noexcept {
            Node* node = pop_free();
            if (!node) {
                std::lock_guard<std::mutex> lock(mtx);
                node = pop_free();
                if (!node) {
                    expand(pool.size() * 2);
                    node = pop_free();
                }
            }
            if (node) {
                node->~Node();
                new (node) Node(std::forward<Args>(args)...);
                node->next.store(nullptr, std::memory_order_relaxed);
            }
            return node;
        }

        void deallocate(Node* node) noexcept {
            if (node) {
                node->version.fetch_add(1, std::memory_order_relaxed);
                node->~Node();
                new (node) Node();
                push_free(node);
            }
        }

    private:
        std::vector<std::unique_ptr<Node>> pool;
        alignas(64) std::atomic<Node*> free_list{ nullptr }; 
        alignas(64) std::mutex mtx;

        void expand(size_t new_size) noexcept {
            if (new_size <= pool.size()) 
                return;
            size_t old_size = pool.size();
            pool.resize(new_size);
            for (size_t i = old_size; i < new_size; ++i) {
                pool[i] = std::make_unique<Node>(); 
                push_free(pool[i].get());
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

public:
    QueueCAS(const QueueCAS&) = delete;
    QueueCAS& operator=(const QueueCAS&) = delete;
};

template<typename ElemType>
QueueCAS<ElemType>::QueueCAS(void) {
    Node* dummy = pool.allocate(); 
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

template<typename ElemType>
QueueCAS<ElemType>::~QueueCAS(void) {
    
}

template<typename ElemType>
bool QueueCAS<ElemType>::enqueue(ElemType elem) noexcept {
    Node* new_node = pool.allocate(std::move(elem));
    if (!new_node) {
        return false;
    }

    Node* old_tail;
    size_t old_version;
    Node* next;

    while (true) {
        old_tail = tail.load(std::memory_order_acquire);
        old_version = old_tail->version.load(std::memory_order_acquire);
        next = old_tail->next.load(std::memory_order_acquire);

        if (tail.load(std::memory_order_acquire) != old_tail ||
            old_tail->version.load(std::memory_order_acquire) != old_version) {
            continue;
        }

        if (next == nullptr) {
            if (old_tail->next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        } else {
            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
        }
    }

    tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release, std::memory_order_relaxed);
    return true;
}

template<typename ElemType>
bool QueueCAS<ElemType>::dequeue(ElemType& result) noexcept {
    // Try a reasonable number of times, balancing success rate and CPU usage
    return try_dequeue(result, 16);
}

template<typename ElemType>
bool QueueCAS<ElemType>::dequeue_once(ElemType& result) noexcept {
    // Only one
    return try_dequeue(result, 1);
}

template<typename ElemType>
bool QueueCAS<ElemType>::try_dequeue(ElemType& result, int max_attempts) noexcept {
    for (int i = 0; i < max_attempts; ++i) {
        Node* old_head = head.load(std::memory_order_acquire);
        size_t old_version = old_head->version.load(std::memory_order_acquire);
        Node* old_tail = tail.load(std::memory_order_acquire);
        Node* next = old_head->next.load(std::memory_order_acquire);

        if (head.load(std::memory_order_acquire) != old_head ||
            old_head->version.load(std::memory_order_acquire) != old_version) {
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
        
        if (!next->elem_initialized) {
            continue;
        }
        
        if (head.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_relaxed)) {
            result = std::move(next->elem);
            pool.deallocate(old_head);
            return true;
        }
    }
	
    return false;
}

template<typename ElemType>
void QueueCAS<ElemType>::dump(void) {
    Node* current = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
    std::cout << "Queue elements: ";

    if (!current) {
        std::cout << "Empty" << std::endl;
        return;
    }

    while (current) {
        if (current->elem_initialized) {
            std::cout << current->elem << " ";
        }
        current = current->next.load(std::memory_order_acquire);
    }
    std::cout << std::endl;
}

#endif // __LockFreeQueue_h__
