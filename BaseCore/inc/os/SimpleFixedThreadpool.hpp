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
#include <unordered_map>
#include <iostream>

//---------------------------------------------------------------------------------------//
__BEGIN__
	
	//------------------------------------------------------------------------------------//
	class fixed_thread_pool {
	public:
		explicit fixed_thread_pool(size_t thread_count)
			: data_(std::make_shared<data>()) {
			for (size_t i = 0; i < thread_count; ++i) {
				std::thread([data = data_] {
					std::unique_lock<std::mutex> lk(data->mtx_);
					for (;;) {
						if (!data->tasks_.empty()) {
							auto func = data->tasks_.front();
							auto iter = std::find_if(data->funParams.begin(), data->funParams.end(),
								[func](const auto& item) {
									void (* const* ptrf)(void*) = func.target<void(*)(void*)>();
									void (* const* ptrf1)(void*) = item.second.target<void(*)(void*)>();
									if (ptrf && ptrf1 && *ptrf == *ptrf1)
										return true;
									return false;
								}
							);
							if (iter != data->funParams.end()) {
								auto currentfunc = std::move(func);
								auto param = iter->first;
								data->funParams.erase(iter);
								data->tasks_.pop();
								lk.unlock();
								currentfunc(param);
								lk.lock();
							}
						}
						else if (data->is_shutdown_) {
							std::cout << std::flush << "thread " << std::this_thread::get_id() << " exit" << std::endl;
							break;
						}
						else {
							data->cond_.wait(lk);
						}
					}
				}).detach();
			}
		}
	
		fixed_thread_pool() = default;
		fixed_thread_pool(fixed_thread_pool&&) = default;
	
		~fixed_thread_pool() {
			if ((bool)data_) {
				{
					std::lock_guard<std::mutex> lk(data_->mtx_);
					data_->is_shutdown_ = true;
				}
				data_->cond_.notify_all();
			}
		}
	
		template <class F>
		void execute(F&& task, void* arg) {
			{
				std::lock_guard<std::mutex> lk(data_->mtx_);
				data_->funParams[arg] = task;
				data_->tasks_.emplace(std::forward<F>(task));
			}
			data_->cond_.notify_one();
		}
	
	private:
		struct data {
			std::mutex mtx_;
			std::condition_variable cond_;
			bool is_shutdown_ = false;
			std::queue<std::function<void(void*)>> tasks_;
			std::unordered_map<void*, std::function<void(void*)>> funParams;
		};
		std::shared_ptr<data> data_;
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
			struct param* cl = (struct param*)args;
			
			// do something
	
			cl->callbackFunc();
			delete cl;
		}
	};
	
	//example:
	//fixed_thread_pool* threadpoolPtr = new fixed_thread_pool(10);
	//threadpoolPtr->execute(std::function<void(void*)>(threadFunc), new struct param());
	//delete threadpoolPtr;

__END__

#endif /*__SimpleFixedThreadpool_h__*/
