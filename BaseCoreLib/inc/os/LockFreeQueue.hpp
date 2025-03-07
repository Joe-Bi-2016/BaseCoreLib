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

template<typename ElemType>
class LockFreeQueue {
public:
    LockFreeQueue(void);
    ~LockFreeQueue(void);

    void push(ElemType elem);
    ElemType pop(void);
    void dump(void);

private:
    typedef struct qNode {
        qNode(void) : next(nullptr) { }
        qNode(ElemType elem) : elem(elem), next(nullptr) { }
        ElemType       elem;
        std::atomic<struct qNode*> next;
    }Node;
    
private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};

template<typename ElemType>
LockFreeQueue<ElemType>::LockFreeQueue(void) {
    head = tail = new Node();
}

template<typename ElemType>
LockFreeQueue<ElemType>::~LockFreeQueue(void) {
    while (head != nullptr)
    {
        Node* tempNode = head;
        head = head->next;
        delete tempNode;
    }
}

template<typename ElemType>
void LockFreeQueue<ElemType>::push(ElemType elem) {
    Node* newNode = new Node(elem);
    Node* oldtail;
    Node* next;

    do {
		oldtail = tail.load(std::memory_order_acquire);
		next = oldtail->next.load(std::memory_order_acquire);
		if (oldtail != tail.load(std::memory_order_relaxed))
			continue;
    
		if (next != nullptr) {
			Node* expected = oldtail;
			tail.atomic_compare_exchange_weak(expected, next, std::memory_order_release, std::memory_order_relaxed);
			continue;
		}
	} while (!oldtail->next.atomic_compare_exchange_weak(next, newNode, std::memory_order_release, std::memory_order_relaxed));

	Node* expected = oldtail;
	tail.compare_exchange_weak(expected, newNode, std::memory_order_release, std::memory_order_relaxed);
}

template<typename ElemType>
ElemType LockFreeQueue<ElemType>::pop(void) {
    Node* oldHead;
    Node* next;
    do {
        oldHead = head.load(std::memory_order_acquire);
        Node* oldTail = tail.load(std::memory_order_acquire);
        next = oldHead->next.load(std::memory_order_acquire);
        if(oldHead != head.load(std::memory_order_relaxed))
            continue;

        if (oldHead == oldTail && next == nullptr) {
            throw std::runtime_error("Queue is empty");
            return  ElemType(0);
        }

    } while (!head.atomic_compare_exchange_weak(oldHead, next, std::memory_order_release, std::memory_order_relaxed));

    ElemType val = oldHead->elem;
    oldHead->next = nullptr;
    delete oldHead;

    return val; 
}

template<typename ElemType>
void LockFreeQueue<ElemType>::dump(void) {
    Node* tempNode = head->next;

    if (tempNode == nullptr) {
        std::cout << "Empty" << std::endl;
        return;
    }

    while (tempNode != nullptr)
    {
        std::cout << tempNode->elem << " ";
        tempNode = tempNode->next;
    }
    std::cout << std::endl;
}

#endif // __LockFreeQueue_h__
