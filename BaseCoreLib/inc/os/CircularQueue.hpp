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
#include <memory>
#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <stdexcept>
#include <chrono>
#include <atomic>
#include <thread>
#include <array>
#include <utility>

//---------------------------------------------------------------------------//
__BEGIN__

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
		inline void cpu_relax() { __asm__ volatile("" ::: "memory"); }
	#endif

    //-----------------------------------------------------------------------//
    template <typename T>
    class CircularQueue {
    public:
        explicit CircularQueue(size_t capacity)
		: capacity_(capacity),
          size_(0),
          head_(0),
          tail_(0),
		  buffer_(new T[capacity])
		{
			static_assert(std::is_default_constructible<T>::value,
							"CircularQueue requires T to be default-constructible");
            static_assert(std::is_nothrow_move_assignable<T>::value,
							"CircularQueue requires T to have noexcept move assignment");
            
			if (capacity == 0)
                throw std::invalid_argument("capacity must be > 0");
		}
		
		CircularQueue(const CircularQueue&) = delete;
		CircularQueue& operator=(const CircularQueue&) = delete;
		CircularQueue(CircularQueue&&) = delete;
		CircularQueue& operator=(CircularQueue&&) = delete;
		~CircularQueue() = default;
		
        bool empty() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
            return size_ == 0;
        }
    
        bool full() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
            return size_ == capacity_;
        }
    
        size_t size() const
		{
			std::lock_guard<std::mutex> lock(mutex_);
            return size_;
        }
    
        size_t capacity() const noexcept
		{
            return capacity_;
        }
    
        bool push(const T& value, bool block = true) 
		{
            // copy to temporary to allow exception-safe copy even without
            // nothrow copy assignment; then move assign
            T tmp(value);
            return push(std::move(tmp), block);
        }
    
        bool push(T&& value, bool block = true) 
		{
            std::unique_lock<std::mutex> lock(mutex_);
    
            if (block) 
                not_full_.wait(lock, [this]{ return size_ < capacity_; });
            else 
			{
                if (size_ == capacity_) 
                    return false;
            }
    
			buffer_[tail_] = std::move(value);
            tail_ = (tail_ + 1) % capacity_;
            ++size_;
    
            not_empty_.notify_one();
    
            return true;
        }
		
		template <class Rep, class Period>
		bool push(const T& value, const std::chrono::duration<Rep, Period>& timeout)
		{
			T tmp(value);
			return push(std::move(tmp), timeout);
		}
	
		template <class Rep, class Period>
		bool push(T&& value, const std::chrono::duration<Rep, Period>& timeout)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			
			if (!not_full_.wait_for(lock, timeout, [this]{ return size_ < capacity_; }))
				return false;
				
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
                not_empty_.wait(lock, [this]{ return size_ > 0; });
            else 
			{
                if (size_ == 0) 
                    return false;
            }
    
			value = std::move(buffer_[head_]);
            head_ = (head_ + 1) % capacity_;
            --size_;
    
            not_full_.notify_one();
    
            return true;
        }
		
		template <class Rep, class Period>
		bool pop(T& value, const std::chrono::duration<Rep, Period>& timeout) 
		{
			std::unique_lock<std::mutex> lock(mutex_);
			
			if (!not_empty_.wait_for(lock, timeout, [this]{ return size_ > 0; }))
				return false;
				
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
		std::unique_ptr<T[]> buffer_;
        mutable std::mutex mutex_;
        std::condition_variable not_full_;
        std::condition_variable not_empty_;
    };
    
    //-----------------------------------------------------------------------//
	// 64-bit counters can run for centuries without overflow.
    template <typename T, size_t N>
    class RingQueue {
    public:
        RingQueue() : read_idx_(0), write_idx_(0), size_(0)
		{ 
			static_assert(N > 0, "RingQueue requires N > 0");
			static_assert((N & (N - 1)) == 0, "RingQueue N must be a power of 2");
			static_assert(std::is_default_constructible<T>::value,
							"RingQueue requires T to be default-constructible");
			static_assert(std::is_nothrow_move_assignable<T>::value,
							"RingQueue requires T to be nothrow move-assignable");
							
			for (size_t i = 0; i < N; ++i) 
				states_[i].value.store(i, std::memory_order_relaxed);
		}
		
		RingQueue(const RingQueue&) = delete;
		RingQueue& operator=(const RingQueue&) = delete;
		RingQueue(RingQueue&&) = delete;
		RingQueue& operator=(RingQueue&&) = delete;
    
        bool push(const T& item, bool block = false) 
		{ 
			T tmp(item);
			return push(std::move(tmp), block);
		}
		
        bool push(T&& item, bool block = false) 
		{ 
			return pushImpl(std::move(item), block); 
		}
		
		template <class Rep, class Period>
		bool push(const T& item, const std::chrono::duration<Rep, Period>& timeout) 
		{
			T tmp(item);
			return push(std::move(tmp), timeout);
		}

		template <class Rep, class Period>
		bool push(T&& item, const std::chrono::duration<Rep, Period>& timeout)
		{
			return pushImpl(std::move(item), timeout);
		}
		
        bool pop(T& item, bool block = false) 
		{
			return popImpl(item, block);
        }
    
        template <typename Func>
        bool pop(Func&& func, bool block = false) 
		{
			static_assert(noexcept(func(std::declval<T>())), 
			"The callback must be noexcept to avoid data loss");
			static_assert(std::is_nothrow_move_constructible<T>::value,
			"RingQueue callback pop requires T to be nothrow move-constructible");
			
            T item;
			if (!popImpl(item, block)) 
				return false;
				
			func(std::move(item));
			
			return true;
        }

		template <class Rep, class Period>
		bool pop(T& item, const std::chrono::duration<Rep, Period>& timeout) 
		{
			return popImpl(item, timeout);
		}
	
		template <class Func, class Rep, class Period>
		bool pop(Func&& func, const std::chrono::duration<Rep, Period>& timeout)
		{
			static_assert(noexcept(func(std::declval<T>())), 
			"The callback must be noexcept to avoid data loss");
			static_assert(std::is_nothrow_move_constructible<T>::value,
			"RingQueue callback pop requires T to be nothrow move-constructible");
			
			T item;
			if (!popImpl(item, timeout)) 
				return false;
        
			func(std::move(item));
			
			return true;
		}
	
        bool isEmpty() const 
		{
            return size_.load(std::memory_order_relaxed) == 0;
        }
    
        bool isFull() const 
		{
            return size_.load(std::memory_order_relaxed) == N;
        }
    
    private:
        template <typename Item>
        bool pushImpl(Item&& item, bool block = false) 
		{
			while (true)
			{
				size_t idx = write_idx_.load(std::memory_order_relaxed);
				size_t slot = idx & (N - 1);  // N is power of 2
				size_t state = states_[slot].value.load(std::memory_order_acquire);
				
				if (state == idx) // slot free for write
				{
					if (write_idx_.compare_exchange_weak(idx, idx + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						// got slot, write data
						data_[slot] = std::forward<Item>(item);
						states_[slot].value.store(idx + 1, std::memory_order_release);
						size_.fetch_add(1, std::memory_order_release);
						return true;
					}
					// CAS failed, retry
					continue;
				}
				
				if (!block)
					return false;
					
				block_wait();
			}
        }
		
		template <typename Item, typename Rep, typename Period>
		bool pushImpl(Item&& item, const std::chrono::duration<Rep, Period>& timeout)
		{
			auto deadline = std::chrono::steady_clock::now() + timeout;
			while (true)
			{
				size_t idx = write_idx_.load(std::memory_order_relaxed);
				size_t slot = idx & (N - 1);
				size_t state = states_[slot].value.load(std::memory_order_acquire);
				
				if (state == idx)
				{
					if (write_idx_.compare_exchange_weak(idx, idx + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						data_[slot] = std::forward<Item>(item);
						states_[slot].value.store(idx + 1, std::memory_order_release);
						size_.fetch_add(1, std::memory_order_release);
						return true;
					}
					continue;
				}
				
				if (std::chrono::steady_clock::now() >= deadline)
					return false;
				
				adaptive_wait();
			}
		}
    
        bool popImpl(T& item, bool block) 
		{
			while (true) 
			{
				size_t idx = read_idx_.load(std::memory_order_relaxed);
				size_t slot = idx & (N - 1);
				size_t state = states_[slot].value.load(std::memory_order_acquire);
				
				if (state == idx + 1) // data ready
				{
					if (read_idx_.compare_exchange_weak(idx, idx + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						item = std::move(data_[slot]);
						states_[slot].value.store(idx + N, std::memory_order_release);
						size_.fetch_sub(1, std::memory_order_release);
						return true;
					}
					continue;
				}
				
				if (!block)
					return false;
					
				block_wait();
			}
		}

		template <typename Rep, typename Period>
		bool popImpl(T& item, const std::chrono::duration<Rep, Period>& timeout)
		{
			auto deadline = std::chrono::steady_clock::now() + timeout;
			while (true)
			{
				size_t idx = read_idx_.load(std::memory_order_relaxed);
				size_t slot = idx & (N - 1);
				size_t state = states_[slot].value.load(std::memory_order_acquire);
				
				if (state == idx + 1)
				{
					if (read_idx_.compare_exchange_weak(idx, idx + 1, std::memory_order_relaxed, std::memory_order_relaxed))
					{
						item = std::move(data_[slot]);
						states_[slot].value.store(idx + N, std::memory_order_release);
						size_.fetch_sub(1, std::memory_order_release);
						return true;
					}
					continue;
				}
				
				if (std::chrono::steady_clock::now() >= deadline)
					return false;
				
				adaptive_wait();
			}
		}
		
		static void block_wait()
		{
			constexpr int max_spins = 1000;
			for (int i = 0; i < max_spins; ++i)
			{
				if (i < max_spins / 2)
					cpu_relax();
				else
					std::this_thread::yield();
			}
	
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}

		// Adaptive waiting: short spin then yield/sleep to avoid wasting CPU
        static void adaptive_wait()
		{
            constexpr int max_spins = 500;
            for (int i = 0; i < max_spins; ++i) 
			{
                if (i < max_spins / 2)
                    cpu_relax(); // tight spin
                else
                    std::this_thread::yield(); // back off
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10)); // final fallback
        }
		
		struct alignas(64) AlignedState 
		{
			std::atomic<size_t> value;
		};


		alignas(64) std::atomic<size_t> read_idx_{0};
		alignas(64) std::atomic<size_t> write_idx_{0};
		alignas(64) std::atomic<size_t> size_{0};
		std::array<AlignedState, N> states_;
		alignas(64) std::array<T, N> data_; 
    };

__END__

#endif // __CircularQueue_h__
