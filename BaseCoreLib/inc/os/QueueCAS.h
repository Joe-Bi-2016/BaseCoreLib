/*****************************************************************************
* FileName    : QueueCAS.h
* Description : Free-lock queues definition, implemented in c++11
* Author      : Joe.Bi
* Date        : 2024-04
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __QueueCAS_h__
#define __QueueCAS_h__
#include <atomic>
#include <iostream>

template<typename ElemType>
class QueueCAS {
public:
    QueueCAS(void);
    ~QueueCAS(void);

    void push(ElemType elem);
    ElemType pop(void);
    void dump(void);

private:
    typedef struct qNode {
        qNode(void) : next(nullptr) { }
        qNode(ElemType elem) : elem(elem), next(nullptr) { }
        ElemType       elem;
        struct qNode* next;
    }Node;
    
private:
    Node* head;    //头结点
    Node* tail;    //尾节点
};

template<typename ElemType>
QueueCAS<ElemType>::QueueCAS(void) {
    head = tail = new Node();
}

template<typename ElemType>
QueueCAS<ElemType>::~QueueCAS(void) {
    while (head != nullptr)
    {
        Node* tempNode = head;
        head = head->next;
        delete tempNode;
    }
}

template<typename ElemType>
void QueueCAS<ElemType>::push(ElemType elem) {
    Node* newNode = new Node(elem);
    
    Node* p = tail;
    Node* oldtail = tail;
    Node* pNull(nullptr);
 
    do {
        while (p->next != nullptr)
            p = p->next;
    } while (atomic_compare_exchange_weak((std::atomic<Node*>*)(&p->next), (Node**)(&pNull), newNode) != true);
    
    atomic_compare_exchange_weak((std::atomic<Node*>*)(&tail), (Node**)(&oldtail), newNode);
}

template<typename ElemType>
ElemType QueueCAS<ElemType>::pop(void) {
    Node* p;
    do {
        p = head->next;
        if (p == nullptr)
            return ElemType(0);
    } while (atomic_compare_exchange_weak((std::atomic<Node*>*)(& head->next), (Node**)&p, p->next) != true);
   
    Node* expected = p;
    atomic_compare_exchange_weak((std::atomic<Node*>*)(&tail), (Node**)&expected, head);  // when expected is not equal to tail, they swap 
    
    ElemType val = p->elem;
    p->next = nullptr;
    delete p;

    return val; 
}

template<typename ElemType>
void QueueCAS<ElemType>::dump(void) {
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

#endif // __QueueCAS_h__
