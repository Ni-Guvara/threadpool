#pragma once
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>

#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <condition_variable>

#include <iostream>

#define  MAX_TASK_NUM 1024
#define  THREAD_IDLE_DURATION 5


class Result;
class Any;

using uLong = unsigned long long;
enum ThreadMode {
	CACHED,
	FIXED
};
class Task {
public:
	Task();
	~Task();

	void setResult(Result* res);
	void exec();
	virtual Any run() = 0;

	Result* res_;
};
/* examples

	Any a(1);
	std::cout << a.cast_<int>();

*/
class Any {
public:	
	Any() = default;
	~Any() = default;
	Any(const Any& v) = delete;
	Any& operator=(const Any& v) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T v) :data(std::make_unique<Derived<T>>(v)) {}

	template<typename T>
	T cast_(){
		Derived<T>* derived = dynamic_cast<Derived<T>*>(data.get());
		
		return derived->val_;
		
	}

private:
	class Base {
	public:
		virtual ~Base() = default;
	};

	template<typename T>
	class Derived : public Base {
	public:
		Derived(T val) :val_(val) {
		}
		~Derived() {
		}

		T val_;
	};


	std::unique_ptr<Base> data;
};
class Semaphore {
private:
	std::atomic_int val_;
	std::mutex mtx_;
	std::condition_variable cond_;

public:
	Semaphore():val_(int(0)){}
	Semaphore(int val) :val_(val) {}

	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cond_.wait(lock, [&]() {
			return val_ > 0; });
		val_--;
		if(val_ > 0)
			cond_.notify_all();
	}

	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		val_++;
		cond_.notify_all();
	}
};

class Result {
public:
	Result(const std::shared_ptr<Task> task, bool isValid = true):task_(task), isValid_(isValid){
		task_->setResult(this);
	}

	~Result(){}

	Result(const Result&) = default;
	void setVal(Any val);
	Any get();

private:
	Any val_;
	bool isValid_;
	Semaphore sem_;
	std::shared_ptr<Task> task_;
};

class Thread {

public:
	using Func = std::function<void(int)>;

	Thread(Func f);
	~Thread();

	void start();
	int  getId();

private:
	Func f;
	int id;
	static int threadId;

};



class ThreadPool {

public:
	ThreadPool();
	~ThreadPool();

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	
	
	void setMode(ThreadMode mode);
	bool checkThreadPoolIsRunning();
	Result submitTask(std::shared_ptr<Task> task);
	void start(int threadTask = std::thread::hardware_concurrency());
	void setTaskQueThreshold(int taskQueThreshold =  MAX_TASK_NUM);
	void setTaskQueCurrentSize(int currentTaskSize);
	void setThreadsQueThreshold(int threadsThreshold);
	void setThreadQueCurrentSize(int currentThreadSize);
	void threadHandler(int threadId);

private:
	ThreadMode                            mode_;
	//std::vector<std::unique_ptr<Thread>>  threadsMap_;      // œÍœ∏—ßœ∞ ÷«ƒ‹÷∏’Î std::unique_ptr
	std::unordered_map<int, std::unique_ptr<Thread>> threadsMap_;
	bool                                  threadPoolIsRunning_;
	int                                   initThreadSize_;
	int                                   currentThreadSize_;
	int                                   threadsQueSizeThreshold_;
	int                                   idleThreadsSize_;

	std::queue<std::shared_ptr<Task>>     tasksQue_;        // œÍœ∏—ßœ∞ ÷«ƒ‹÷∏’Î std::shared_ptr
	std::atomic_int                       currentTaskSize_;
	int									  taskQueSizeThreshold_;

	std::mutex                            taskQueMtx;
	std::condition_variable               taskQueNotEmpty;
	std::condition_variable               taskQueNotFull;

	std::condition_variable               conExit_;
};

#endif
