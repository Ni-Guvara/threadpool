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
#include <unordered_map>
#include <iostream>
#include <future>

using namespace std;

#define MAX_TASK_NUM 1024
#define THREAD_IDLE_DURATION 5

class Result;
class Any;

using uLong = unsigned long long;

enum ThreadMode
{
	CACHED,
	FIXED
};

class Thread
{

public:
	using Func = std::function<void(int)>;

	Thread(Func f) : id(threadId++)
	{
		this->f = f;
	}

	~Thread()
	{
	}

	void start()
	{
		std::thread t(this->f, id);
		t.detach();
	}

	int getId()
	{
		return id;
	}

private:
	Func f;
	int id;
	static int threadId;
};

int Thread::threadId = 0;

class ThreadPool
{

public:
	ThreadPool() : threadPoolIsRunning_(false),
				   mode_(ThreadMode::FIXED),
				   threadsQueSizeThreshold_(16),
				   initThreadSize_(5),
				   currentThreadSize_(5),
				   idleThreadsSize_(0),
				   taskQueSizeThreshold_(MAX_TASK_NUM),
				   currentTaskSize_(0)
	{
	}

	~ThreadPool()
	{
		this->threadPoolIsRunning_ = false;
		std::unique_lock<std::mutex> lock(this->taskQueMtx);
		taskQueNotEmpty.notify_all();
		conExit_.wait(lock, [&]() -> bool
					  { return threadsMap_.size() == 0; });
	}

	ThreadPool(const ThreadPool &) = delete;
	ThreadPool &operator=(const ThreadPool &) = delete;

	void setMode(ThreadMode mode)
	{
		mode_ = mode;
	}

	bool checkThreadPoolIsRunning()
	{
		return threadPoolIsRunning_;
	}

	void setTaskQueThreshold(int taskQueSizeThreshold = MAX_TASK_NUM)
	{
		if (!checkThreadPoolIsRunning())
			this->taskQueSizeThreshold_ = taskQueSizeThreshold;
	}

	void setTaskQueCurrentSize(int currentTasksSize)
	{
		if (!checkThreadPoolIsRunning())
			this->currentTaskSize_ = currentTasksSize;
	}

	void setThreadsQueThreshold(int threadsQueSizeThreshold)
	{
		if (!checkThreadPoolIsRunning())
			this->threadsQueSizeThreshold_ = threadsQueSizeThreshold;
	}

	void setThreadQueCurrentSize(int currentThreadsSize)
	{
		if (!checkThreadPoolIsRunning())
			this->currentThreadSize_ = currentThreadsSize;
	}

	void start(int threadSize = std::thread::hardware_concurrency())
	{
		this->threadPoolIsRunning_ = true;

		this->initThreadSize_ = threadSize;
		this->currentThreadSize_ = threadSize;

		for (int i = 0; i < this->currentThreadSize_; i++)
		{

			auto f = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);

			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);

			this->threadsMap_.emplace(ptr->getId(), std::move(ptr));
		}

		for (int i = 0; i < this->currentThreadSize_; i++)
		{
			this->threadsMap_[i]->start();
			this->idleThreadsSize_++;
		}
	}

	// 可变参模板编程，让函数接收任意任务函数和任意数量参数
	template <typename Func, typename... Args>
	auto submitTask(Func &&func, Args &&...args) -> std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

		std::future<RType> result = task->get_future();

		std::cout << "Submit Task" << std::endl;

		std::unique_lock<std::mutex> lock(this->taskQueMtx);

		if (!this->taskQueNotFull.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
										   { return (this->currentTaskSize_ < this->taskQueSizeThreshold_); }))
		{
			std::cout << "Task Queue Is Full" << endl;
			auto task = std::make_shared<std::packaged_task<RType()>>([]() -> RType
																	  { return RType(); });
			(*task)();

			return task->get_future();
		}

		// 加入中间层，在void()中执行task()
		auto taskWrapper = std::make_shared<Task>([task]()
												  { (*task)(); });
		tasksQue_.emplace(taskWrapper);

		currentTaskSize_++;
		this->taskQueNotEmpty.notify_all();

		if (mode_ == ThreadMode::CACHED)
		{
			if (this->currentTaskSize_ > this->idleThreadsSize_ &&
				this->currentThreadSize_ < this->threadsQueSizeThreshold_)
			{

				auto f = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);

				std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);
				int id = ptr->getId();

				this->threadsMap_.emplace(id, std::move(ptr));

				this->threadsMap_[id]->start();
				this->currentThreadSize_++;
				this->idleThreadsSize_++;
			}
		}

		return result;
	}

	void threadHandler(int threadId)
	{
		// std::chrono::time_point last = std::chrono::system_clock::now();
		auto last = std::chrono::system_clock::now();

		for (;;)
		{
			shared_ptr<Task> task;
			{
				std::cout << "Thread " << threadId << std::endl;

				std::unique_lock<std::mutex> lock(this->taskQueMtx);

				while (currentTaskSize_ == 0)
				{
					if (!threadPoolIsRunning_)
					{
						std::cout << "Thread  " << threadId << "exit!!!";
						threadsMap_.erase(threadId);
						conExit_.notify_all();
						return;
					}

					if (this->mode_ == ThreadMode::CACHED)
					{

						if (std::cv_status::timeout == taskQueNotEmpty.wait_for(lock, std::chrono::seconds(1)))
						{
							auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last);
							if (duration.count() > THREAD_IDLE_DURATION && currentThreadSize_ > this->initThreadSize_)
							{
								std::cout << "Thread  " << threadId << "exit!!!";
								threadsMap_.erase(threadId);
								this->currentThreadSize_--;
								this->idleThreadsSize_--;

								return;
							}
						}
					}
					else
					{
						taskQueNotEmpty.wait(lock);
					}
				}

				task = this->tasksQue_.front();
				this->tasksQue_.pop();
				this->currentTaskSize_--;
				this->idleThreadsSize_--;

				if (this->currentTaskSize_ > 0)
					taskQueNotEmpty.notify_all();
				taskQueNotFull.notify_all();
			}
			std::cout << "Thread " << threadId << std::endl;

			if (task != nullptr)
				(*task)();

			this->idleThreadsSize_++;
			last = std::chrono::system_clock::now();
		}
	}

private:
	ThreadMode mode_;
	// std::vector<std::unique_ptr<Thread>>  threadsMap_;
	unordered_map<int, std::unique_ptr<Thread>> threadsMap_;
	bool threadPoolIsRunning_;
	int initThreadSize_;
	int currentThreadSize_;
	int threadsQueSizeThreshold_;
	int idleThreadsSize_;

	using Task = function<void()>;
	std::queue<std::shared_ptr<Task>> tasksQue_;
	std::atomic_int currentTaskSize_;
	int taskQueSizeThreshold_;

	std::mutex taskQueMtx;
	std::condition_variable taskQueNotEmpty;
	std::condition_variable taskQueNotFull;

	std::condition_variable conExit_;
};

#endif
