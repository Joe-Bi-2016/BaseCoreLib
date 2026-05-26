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

//---------------------------------------------------------------------------//
__BEGIN__
	
	//-----------------------------------------------------------------------//
	class fixed_thread_pool {
	public:
		explicit fixed_thread_pool(size_t thread_count)
			: data_(std::make_shared<data>()) {
			threads_.reserve(thread_count);		
			for (size_t i = 0; i < thread_count; ++i) {
				threads_.emplace_back([data = data_] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lk(data->mtx_);
                        data->cond_.wait(lk, [&] { return data->is_shutdown_ || !data->tasks_.empty();});
                        if (data->is_shutdown_ && data->tasks_.empty())
                            break;
                        task = std::move(data->tasks_.front());
                        data->tasks_.pop();
                    }
                    try {
                        task();
                    } catch (...) {
						std::string msg = "Unknown exception";
                        try {
                            std::rethrow_exception(std::current_exception());
                        } catch (const std::exception& e) {
                            msg = e.what();
                        } catch (...){ }
                        
						std::function<void(const char*)> handler;
						{
							std::lock_guard<std::mutex> lk(data_->mtx_);
							handler = data_->on_exception;
						}

                        if (handler) {
                            handler(msg.c_str());
                        } else {
                            std::cerr << "Exception in thread pool: " << msg << '\n';
                        }
                    }
                }
            });
			}
		}
	
		fixed_thread_pool() noexcept = default;
		fixed_thread_pool(const fixed_thread_pool&) = delete;
		fixed_thread_pool& operator=(const fixed_thread_pool&) = delete;
	
		fixed_thread_pool(fixed_thread_pool&& other) noexcept
        : data_(std::move(other.data_)),
          threads_(std::move(other.threads_)) { 
		}
		
		fixed_thread_pool& operator=(fixed_thread_pool&& other) noexcept {
			if (this != &other) {
				shutdown_and_join();
				data_ = std::move(other.data_);
				threads_ = std::move(other.threads_);
			}
			return *this;
		}
	
		~fixed_thread_pool() {
			shutdown_and_join();
		}
		
		void set_exception_handler(std::function<void(const char*)> handler) {
			if (data_) {
				std::lock_guard<std::mutex> lk(data_->mtx_);
				data_->on_exception = std::move(handler);
			}
		}
		
		explicit operator bool() const noexcept {
			return data_ != nullptr && !threads_.empty();
		}
	
		template <class F>
		void execute(F&& task, void* arg) {
			if (!data_)
				throw std::runtime_error("fixed_thread_pool: execute called on moved-from object");
			{
				std::lock_guard<std::mutex> lk(data_->mtx_);
				data_->tasks_.emplace([task = std::forward<F>(task), arg] { task(arg); });
			}
			data_->cond_.notify_one();
		}
	
	private:
		struct data {
			std::mutex mtx_;
			std::condition_variable cond_;
			bool is_shutdown_ = false;
			std::queue<std::function<void()>> tasks_;
			std::function<void(const char*)> on_exception;
		};
		std::shared_ptr<data> data_;
		std::vector<std::thread> threads_;
		
		void shutdown_and_join() {
			if (data_) {
				{
					std::lock_guard<std::mutex> lk(data_->mtx_);
					data_->is_shutdown_ = true;
				}
				data_->cond_.notify_all();
			}
			
			for (auto& t : threads_) {
				if (t.joinable()) t.join();
			}
		}
	};
	
	struct param {
		param(void) = default;
		virtual ~param(void) = default;
	
		virtual void callbackFunc(void) {
			std::cout << std::flush << "Parameter object " << this << " running callback function" << std::endl;
		}
	};
	
	void threadFunc(void* args) {
		if (args) {
			auto* cl = static_cast<param*>(args);
			
			// do something
	
			cl->callbackFunc();
			delete cl;
		}
	}
	
	//example:
	//fixed_thread_pool pool(10);
	// pool.set_exception_handler([](const char* msg) {
	//     std::cerr << "Custom handler: " << msg << '\n';
	// });

	// pool.execute(std::function<void(void*)>(threadFunc), new param());

	// move
	// fixed_thread_pool pool2 = std::move(pool);
	// if (!pool) { /* pool no longer valid */ }

__END__

#endif /*__SimpleFixedThreadpool_h__*/
