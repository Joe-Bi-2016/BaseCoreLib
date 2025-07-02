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
#include <stdexcept>
#include <memory>

template<typename ElemType>
class QueueCAS {
public:
    QueueCAS(void);
    ~QueueCAS(void);

    void enqueue(ElemType elem);
    ElemType dequeue(void);
    void dump(void);

private:
    typedef struct qNode {
        ElemType elem;
        std::atomic<struct qNode*> next;
        std::atomic<size_t> version; 

        qNode(void) : next(nullptr), version(0) {}
        explicit qNode(ElemType elem) : elem(elem), next(nullptr), version(0) {}
    } Node;

    class MemoryPool {
    public:
        explicit MemoryPool(size_t init_size = 1024) {
            expand(init_size);
        }

        ~MemoryPool() {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto& node : pool) {
                node.reset();
            }
        }

        Node* allocate(ElemType elem) {
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
                node->elem = elem; 
                node->next = nullptr;
            }
            return node;
        }

        void deallocate(Node* node) {
            if (node) {
                node->version.fetch_add(1, std::memory_order_relaxed);
                push_free(node);
            }
        }

    private:
        std::vector<std::unique_ptr<Node>> pool; 
        std::atomic<Node*> free_list{ nullptr }; 
        std::mutex mtx;

        void expand(size_t new_size) {
            if (new_size <= pool.size()) 
                return;
            size_t old_size = pool.size();
            pool.resize(new_size);
            for (size_t i = old_size; i < new_size; ++i) {
                pool[i] = std::make_unique<Node>(); 
                push_free(pool[i].get());
            }
        }

        void push_free(Node* node) {
            Node* old_head = free_list.load(std::memory_order_relaxed);
            do {
                node->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list.compare_exchange_weak(old_head, node, std::memory_order_release, std::memory_order_relaxed));
        }

        Node* pop_free() {
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
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    MemoryPool pool;

public:
    // 禁止拷贝构造和赋值
    QueueCAS(const QueueCAS&) = delete;
    QueueCAS& operator=(const QueueCAS&) = delete;
};

template<typename ElemType>
QueueCAS<ElemType>::QueueCAS(void) {
    Node* dummy = pool.allocate(ElemType());
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

template<typename ElemType>
QueueCAS<ElemType>::~QueueCAS(void) {
    Node* current = head.load(std::memory_order_relaxed);
    while (current) {
        Node* next = current->next.load(std::memory_order_relaxed);
        pool.deallocate(current);
        current = next;
    }
}

template<typename ElemType>
void QueueCAS<ElemType>::enqueue(ElemType elem) {
    Node* new_node = pool.allocate(elem);
    if (!new_node) {
        throw std::bad_alloc();
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
        }
        else {
            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
        }
    }

    tail.compare_exchange_weak(old_tail, new_node, std::memory_order_release, std::memory_order_relaxed);
}

template<typename ElemType>
ElemType QueueCAS<ElemType>::dequeue(void) {
    Node* old_head;
    size_t old_version;
    Node* next;
    Node* old_tail;

    while (true) {
        old_head = head.load(std::memory_order_acquire);
        old_version = old_head->version.load(std::memory_order_acquire);
        old_tail = tail.load(std::memory_order_acquire);
        next = old_head->next.load(std::memory_order_acquire);

        if (head.load(std::memory_order_acquire) != old_head ||
            old_head->version.load(std::memory_order_acquire) != old_version) {
            continue;
        }

        if (old_head == old_tail) {
            if (next == nullptr) {
                throw std::runtime_error("Queue is empty");
            }
            tail.compare_exchange_weak(old_tail, next, std::memory_order_release, std::memory_order_relaxed);
        }
        else {
            if (head.compare_exchange_weak(old_head, next, std::memory_order_release, std::memory_order_relaxed)) {
                break; 
            }
        }
    }

    ElemType val = next->elem;
    pool.deallocate(old_head);

    return val;
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
        ElemType elem = current->elem;
        std::cout << elem << " ";
        Node* next = current->next.load(std::memory_order_acquire);
        current = next;
    }
    std::cout << std::endl;
}

#endif // __LockFreeQueue_h__
