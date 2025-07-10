/*****************************************************************************
* FileName    : CircularQueue.hpp
* Description : Circular queue definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __CircularQueue_h__
#define __CircularQueue_h__
#include "../base/Macro.h"
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

//---------------------------------------------------------------------------//
__BEGIN__

    //-----------------------------------------------------------------------//
    template <typename T>
    class CircularQueue {
    public:
        explicit CircularQueue(size_t capacity) :
            capacity_(capacity),
            size_(0),
            head_(0),
            tail_(0),
            buffer_(new T[capacity]) {}
    
        ~CircularQueue() 
		{
            delete[] buffer_;
        }
    
        bool empty() 
		{
            std::unique_lock<std::mutex> lock(mutex_);
            return size_ == 0;
        }
    
        bool full() 
		{
            std::unique_lock<std::mutex> lock(mutex_);
            return size_ == capacity_;
        }
    
        size_t size() 
		{
            std::unique_lock<std::mutex> lock(mutex_);
            return size_;
        }
    
        size_t capacity() 
		{
            return capacity_;
        }
    
        bool push(const T& value, bool block = true) 
		{
            std::unique_lock<std::mutex> lock(mutex_);
    
            if (block) 
			{
                while (size_ == capacity_) 
				{
                    not_full_.wait(lock);
                }
            }
            else 
			{
                if (size_ == capacity_) 
				{
                    return false;
                }
            }
    
            buffer_[tail_] = value;
            tail_ = (tail_ + 1) % capacity_;
            ++size_;
    
            not_empty_.notify_one();
    
            return true;
        }
    
        bool push(T&& value, bool block = true) 
		{
            std::unique_lock<std::mutex> lock(mutex_);
    
            if (block) 
			{
                while (size_ == capacity_) 
				{
                    not_full_.wait(lock);
                }
            }
            else 
			{
                if (size_ == capacity_)
				{
                    return false;
                }
            }
    
            buffer_[tail_] = std::move(value);
            tail_ = (tail_ + 1) % capacity_;
            ++size_;
    
            not_empty_.notify_one();
    
            return true;
        }
    
        bool pop(T& value, bool block = true) 
		{
            std::unique_lock<std::mutex> lock(mutex_);
    
            if (block) 
			{
                while (size_ == 0)
				{
                    not_empty_.wait(lock);
                }
            }
            else 
			{
                if (size_ == 0) 
				{
                    return false;
                }
            }
    
            value = std::move(buffer_[head_]);
            head_ = (head_ + 1) % capacity_;
            --size_;
    
            not_full_.notify_one();
    
            return true;
        }
    
    private:
        const size_t capacity_; 
        size_t size_; 
        size_t head_;
        size_t tail_;
        T* buffer_;
        std::mutex mutex_;
        std::condition_variable not_full_;
        std::condition_variable not_empty_;
    };
    
    //-----------------------------------------------------------------------//
    template <typename T, size_t N>
    class RingQueue {
    public:
        RingQueue() : read_idx_(0), write_idx_(0), data_{} {}
    
        bool push(const T& item, bool block = false) { return pushImpl(item, block); }
        bool push(T&& item, bool block = false) { return pushImpl(std::move(item), block); }
    
        bool pop(T& item, bool block = false) 
		{
            size_t current_read_idx = read_idx_.load(std::memory_order_relaxed);
    
            while (current_read_idx == write_idx_.load(std::memory_order_acquire))
			{
                if (!block)
				{
                    return false;
                }
                std::this_thread::yield();
            }
    
            item = std::move(data_[current_read_idx]); 
            read_idx_.store(Next(current_read_idx), std::memory_order_release);
    
            return true;
        }
    
        template <typename Func>
        bool pop(Func&& func, bool block = false) 
		{
            size_t current_read_idx = read_idx_.load(std::memory_order_relaxed);
    
            while (current_read_idx == write_idx_.load(std::memory_order_acquire)) 
			{
                if (!block) 
				{
                    return false;
                }
                std::this_thread::yield();
            }
    
            T item = std::move(data_[current_read_idx]);
            read_idx_.store(Next(current_read_idx), std::memory_order_release);
    
            func(std::move(item));
    
            return true;
        }
    
        bool isEmpty() const 
		{
            return read_idx_.load(std::memory_order_acquire) ==
                write_idx_.load(std::memory_order_acquire);
        }
    
        bool isFull() const 
		{
            return Next(write_idx_.load(std::memory_order_acquire)) ==
                read_idx_.load(std::memory_order_acquire);
        }
    
    private:
        template <typename Item>
        bool pushImpl(Item&& item, bool block = false) 
		{
            size_t current_write_idx = write_idx_.load(std::memory_order_relaxed);
            size_t next_write_idx = Next(current_write_idx);
    
            while (next_write_idx == read_idx_.load(std::memory_order_acquire)) 
			{
                if (!block) 
				{
                    return false;
                }
                std::this_thread::yield();
            }
    
            data_[current_write_idx] = std::forward<Item>(item);
    
            write_idx_.store(next_write_idx, std::memory_order_release);
    
            return true;
        }
    
        size_t Next(size_t current_idx) const { return (current_idx + 1) % (N + 1); }
    
        std::atomic<size_t> read_idx_;
        std::atomic<size_t> write_idx_;
        std::array<T, N + 1> data_; 
    };

__END__

#endif // __CircularQueue_h__
