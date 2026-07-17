/*****************************************************************************
* FileName    : SimpleFixedThreadpool.hpp
* Description : Simple fixed thread pool definition
* Author      : Joe.Bi
* Date        : 2023-12
* Version     : v1.0
* Copyright (c)  xxx . All rights reserved.
******************************************************************************/
#ifndef __SimpleFixedThreadpool_h__
#define __SimpleFixedThreadpool_h__
#include "../base/Macro.h"
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <exception>
#include <chrono>
#include <string>

//---------------------------------------------------------------------------//
__BEGIN__
	
	//-----------------------------------------------------------------------//
	class fixed_thread_pool 
	{
	public:
		explicit fixed_thread_pool(size_t thread_count = 10, size_t max_queue_size = 0)
        : data_(std::make_shared<data>(max_queue_size)),
          thread_count_(thread_count)
		{
			try 
			{
				if (thread_count == 0)
					throw std::invalid_argument("thread_count must be > 0");
  
				threads_.reserve(thread_count);
			
				for (size_t i = 0; i < thread_count; ++i)
				{
					threads_.emplace_back([data = data_] {
						{
							std::lock_guard<std::mutex> lk(data->mtx_);
							++data->active_count;
						}
						
						// RAII guard: ensures it will definitely decrement when leaving the scope,
						// whether or not it's abnormal
						struct thread_exit_guard 
						{
							std::shared_ptr<data> data_;
							~thread_exit_guard() 
							{
								std::lock_guard<std::mutex> lk(data_->mtx_);
								--data_->active_count;
								data_->exit_cond_.notify_all();
							}
						} guard{data};
						
						try
						{
							while (true) 
							{
								std::function<void()> task;
								{
									std::unique_lock<std::mutex> lk(data->mtx_);
									data->cond_.wait(lk, [&] { return data->is_shutdown_ || !data->tasks_.empty();});
									if (data->is_shutdown_ && data->tasks_.empty())
										break;
									task = std::move(data->tasks_.front());
									data->tasks_.pop();
									
									if (data->max_queue_size_ > 0)
										data->full_cond_.notify_one();
	
									if (!data->tasks_.empty())
										data->cond_.notify_one();
								}
								try 
								{
									task();
								} 
								catch (...) 
								{
									std::string msg = "Unknown exception";
									try 
									{
										std::rethrow_exception(std::current_exception());
									} 
									catch (const std::exception& e) 
									{
										msg = e.what();
									} 
									catch (...)
									{ }
									
		
									std::function<void(const std::string&)> handler;
									try 
									{
										std::lock_guard<std::mutex> lk(data->mtx_);
										handler = data->on_exception;
									} 
									catch (...) 
									{
										std::cerr << "Exception error in thread pool: get user exception function, original exception: " << msg << '\n';
									}
								
									if (handler)
									{
										try 
										{
											handler(msg);
										} 
										catch (...) 
										{
											std::cerr << "User exception handler threw an unknown exception, original exception: " << msg << '\n';
										}
									} 
									else
										std::cerr << "Exception in thread pool: " << msg << '\n';
								}
							}
						}
						catch(...)
						{
							std::cerr << "Fatal error in thread pool worker, thread exiting." << std::endl;
						}
					});
				}
			} 
			catch(...) 
			{
				shutdown_and_join();
				throw;
			}
		}
	
		fixed_thread_pool() noexcept = default;
		fixed_thread_pool(const fixed_thread_pool&) = delete;
		fixed_thread_pool& operator=(const fixed_thread_pool&) = delete;
	
		fixed_thread_pool(fixed_thread_pool&& other) noexcept
        : data_(std::move(other.data_)),
          threads_(std::move(other.threads_)),
          thread_count_(other.thread_count_)
		{
			other.data_.reset();
			other.threads_.clear();
			other.thread_count_ = 0;
		}

		fixed_thread_pool& operator=(fixed_thread_pool&& other) noexcept 
		{
			if (this != &other) 
			{
				shutdown_and_join(); 
				data_ = std::move(other.data_);
				threads_ = std::move(other.threads_);
				thread_count_ = other.thread_count_;
				other.data_.reset();
				other.threads_.clear();
				other.thread_count_ = 0;
			}
			return *this;
		}
	
		~fixed_thread_pool() 
		{
			shutdown_and_join();
		}
		
		void set_exception_handler(std::function<void(const std::string&)> handler)
		{
			if (data_) 
			{
				std::lock_guard<std::mutex> lk(data_->mtx_);
				data_->on_exception = std::move(handler);
			}
		}
		
		size_t thread_count() const noexcept 
		{
			return thread_count_;
		}
		
		size_t active_count() const 
		{
			if (!data_) 
				return 0;

			std::lock_guard<std::mutex> lk(data_->mtx_);
			return data_->active_count;
		}

		size_t queue_size() const 
		{
			if (!data_) 
				return 0;

			std::lock_guard<std::mutex> lk(data_->mtx_);
			return data_->tasks_.size();
		}
	
		bool is_shutdown() const
		{
			if (!data_) 
				return true;

			std::lock_guard<std::mutex> lk(data_->mtx_);
			return data_->is_shutdown_;
		}
	
		// cleanup function should be noexcept
		template <class F>
		void execute(F&& task, void* arg, std::function<void(void*)> cleanup = nullptr) 
		{
			if (!data_)
				throw std::runtime_error("fixed_thread_pool: execute called on moved-from object");
			
			auto taskPtr = std::make_shared<typename std::decay<F>::type>(std::forward<F>(task));
			std::function<void()> wrapped = [taskPtr, arg, cleanup = std::move(cleanup)] {
				try 
				{
					(*taskPtr)(arg);
				} 
				catch (...)
				{
                // Capture original exception, then run cleanup (if any)
                std::exception_ptr original = std::current_exception();
                if (cleanup)
                {
                    try 
                    {
                        cleanup(arg);
                    }
                    catch (...)
                    {
                        // Suppress cleanup exceptions original exception must be preserved
                    }
                }
                std::rethrow_exception(original);
            }
			};
	
			{
				std::unique_lock<std::mutex> lk(data_->mtx_);

				if (data_->is_shutdown_)
					throw std::runtime_error("fixed_thread_pool: pool is shut down");

				// block waiting
				if (data_->max_queue_size_ > 0) 
				{
					data_->full_cond_.wait(lk, [&] {
						return data_->is_shutdown_ || data_->tasks_.size() < data_->max_queue_size_; });
					
					if (data_->is_shutdown_) 
						throw std::runtime_error("fixed_thread_pool: pool shut down during enqueue");
				}

				// Optimization: Only notify when the queue is empty to avoid unnecessary wake-ups
				bool was_empty = data_->tasks_.empty();
				data_->tasks_.push(std::move(wrapped));
				if (was_empty)
					data_->cond_.notify_one();
			}
		}
		
		// cleanup function should be noexcept
		template<class F>
		bool execute_for(F&& task, void* arg, std::chrono::milliseconds timeout, std::function<void(void*)> cleanup = nullptr)
		{
			if (!data_)
				return false;
			
			auto taskPtr = std::make_shared<typename std::decay<F>::type>(std::forward<F>(task));
			std::function<void()> wrapped = [taskPtr, arg, cleanup = std::move(cleanup)] {
				try 
				{
					(*taskPtr)(arg);
				} 
				catch (...)
				{
                std::exception_ptr original = std::current_exception();
                if (cleanup)
                {
                    try 
                    {
                        cleanup(arg);
                    }
                    catch (...) {}
                }
                std::rethrow_exception(original);
            }
			};
	
			{
				std::unique_lock<std::mutex> lk(data_->mtx_);
	
				if (data_->is_shutdown_)
					return false;
	
				if (data_->max_queue_size_ > 0) 
				{
					bool success = data_->full_cond_.wait_for(lk, timeout, [&] {
						return data_->is_shutdown_ || data_->tasks_.size() < data_->max_queue_size_; });
						
					if (!success || data_->is_shutdown_)
						return false;
				}
	
				// Optimization: Only notify when the queue is empty to avoid unnecessary wake-ups
				bool was_empty = data_->tasks_.empty();
				data_->tasks_.push(std::move(wrapped));
				if (was_empty)
					data_->cond_.notify_one();
			}
			return true;
		}
		
		void shutdown()
		{
			if (data_) 
			{
				{
					std::lock_guard<std::mutex> lk(data_->mtx_);
					data_->is_shutdown_ = true;
				}
				
				data_->cond_.notify_all();
				if (data_->max_queue_size_ > 0)
					data_->full_cond_.notify_all();
			}			
		}
	
	private:
		struct data 
		{
			explicit data(size_t max_q) : max_queue_size_(max_q) {}
			std::mutex mtx_;
			std::condition_variable cond_;
			std::condition_variable full_cond_;
			bool is_shutdown_ = false;
			std::queue<std::function<void()>> tasks_;
			size_t max_queue_size_;
			std::function<void(const std::string&)> on_exception;
			int active_count = 0;
			std::condition_variable exit_cond_;
		};
		
		std::shared_ptr<data> data_;
		std::vector<std::thread> threads_;
		size_t thread_count_ = 0;
		
		void shutdown_and_join() noexcept 
		{
			shutdown();			
			for (auto& t : threads_)
			{
				if (t.joinable()) 
				{
					try 
					{
						t.join();
					} 
					catch (...) 
					{
						try 
						{
							t.detach();
						} 
						catch (...) 
						{ }
					}
				}
			}
			
			if (data_) 
			{
				std::unique_lock<std::mutex> lk(data_->mtx_);
				data_->exit_cond_.wait(lk, [this] { return data_->active_count == 0; });
			}
		}
	};
	
	struct param 
	{
		param(void) = default;
		virtual ~param(void) = default;
	
		virtual void callbackFunc(void)
		{
			std::cout << std::flush << "Parameter object " << this << " running callback function" << std::endl;
		}
	};
	
	void threadFunc(void* args)
	{
		if (args) 
		{
			auto* cl = static_cast<param*>(args);
			
			// do something
	
			cl->callbackFunc();
			delete cl;
		}
	}
	
	//example:
	//fixed_thread_pool pool(10, 100);
	// pool.set_exception_handler([](const char* msg) 
	// {
	//     std::cerr << "Custom handler: " << msg << '\n';
	// });

	// pool.execute(std::function<void(void*)>(threadFunc), new param());

	// move
	// fixed_thread_pool pool2 = std::move(pool);
	// if (!pool) { /* pool no longer valid */ }

__END__

#endif /*__SimpleFixedThreadpool_h__*/
