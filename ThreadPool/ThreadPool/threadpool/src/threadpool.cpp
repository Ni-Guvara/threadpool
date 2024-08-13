#include "threadpool.h"

void Result::setVal(Any v)
{
	val_ = std::move(v);
	sem_.post();
}
Any Result::get()
{
	if (!isValid_)
		return "";
	sem_.wait();
	return std::move(val_);
}
Task::Task():res_(nullptr)
{

}
Task::~Task()
{

}
void Task::setResult(Result* res)
{
	res_ = res;
}
void Task::exec()
{
	if (res_ != nullptr)
	{
		res_->setVal(run());
	}
}

int Thread::threadId = 0;
Thread::Thread(Func f) : id(threadId++)
{
	this->f = f;
}

Thread::~Thread()
{

}

void Thread::start()
{
	std::thread t(this->f, id);
	t.detach();
}

int Thread::getId()
{
	return id;
}

ThreadPool::ThreadPool() :
threadPoolIsRunning_(false),
mode_(ThreadMode::FIXED),
threadsQueSizeThreshold_(16),
initThreadSize_(5),
currentThreadSize_(5),
idleThreadsSize_(0),
taskQueSizeThreshold_(MAX_TASK_NUM),
currentTaskSize_(0)
{
	
}

ThreadPool::~ThreadPool()
{
	this->threadPoolIsRunning_ = false;
	std::unique_lock<std::mutex> lock(this->taskQueMtx);
	taskQueNotEmpty.notify_all();
	conExit_.wait(lock, [&]()->bool {
		return threadsMap_.size() == 0;
		});
}
void ThreadPool::setMode(ThreadMode mode)
{
	this->mode_ = mode;
}
bool ThreadPool::checkThreadPoolIsRunning()
{
	return threadPoolIsRunning_;
}
void ThreadPool::setTaskQueThreshold(int taskQueSizeThreshold)
{
	if(!checkThreadPoolIsRunning())
		this->taskQueSizeThreshold_ = taskQueSizeThreshold;
}
void ThreadPool::setTaskQueCurrentSize(int currentTasksSize)
{
	if (!checkThreadPoolIsRunning())
		this->currentTaskSize_ = currentTasksSize;
}
void ThreadPool::setThreadsQueThreshold(int threadsQueSizeThreshold)
{
	if (!checkThreadPoolIsRunning())
		this->threadsQueSizeThreshold_ = threadsQueSizeThreshold;
}
void ThreadPool::setThreadQueCurrentSize(int currentThreadsSize)
{
	if (!checkThreadPoolIsRunning())
		this->currentThreadSize_ = currentThreadsSize;
}

void ThreadPool::start(int threadSize)
{
	this->threadPoolIsRunning_ = true;
	// 成员变量赋值
	this->initThreadSize_ = threadSize;
	this->currentThreadSize_ = threadSize;
	
	// 线程队列添加initThreadSize 个Thread线程
	for (int i = 0; i < this->currentThreadSize_; i++)
	{
		// 使用智能指针 std::unique_ptr
		auto f  = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);
		// 绑定函数并放入Thread中
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);
		// emplace_back添加
		this->threadsMap_.emplace(ptr->getId(), std::move(ptr));
	}
	// 开启线程
	for (int i = 0; i < this->currentThreadSize_; i++)
	{
		this->threadsMap_[i]->start();
		this->idleThreadsSize_++;
	}
}

// 提交任务
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	std::cout << "添加任务" << std::endl;
	// 添加互斥锁
	std::unique_lock<std::mutex> lock(this->taskQueMtx);

	// 条件变量判断
	if (!this->taskQueNotFull.wait_for(lock, std::chrono::seconds(1), [&]()->bool {
		return (this->currentTaskSize_ < this->taskQueSizeThreshold_);
		}))
	{
		std::cerr << "队列已满，提交失败" << std::endl;
		return Result(task, false);
	}

	this->tasksQue_.emplace(task);
	this->currentTaskSize_++;
	this->taskQueNotEmpty.notify_all();

	// CACHED模式下，当线程数量小于当前的任务数量同时线程数量小于线程阈值数量的时候，创建线程
	if (mode_ == ThreadMode::CACHED)
	{
		if (this->currentTaskSize_ > this->idleThreadsSize_ &&
			this->currentThreadSize_ < this->threadsQueSizeThreshold_)
		{
			// 绑定函数
			auto f = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);
			// 创建 std::unique_ptr智能指针并将绑定函数放入Thread线程对象中
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);
			int id = ptr->getId();
			std::cout << "Thread " << id << "创建" << std::endl;
			// 将创建好的thread对象移动拷贝到线程队列中
			this->threadsMap_.emplace(id, std::move(ptr));
			// 启动创建的对象
			this->threadsMap_[id]->start();
			this->currentThreadSize_++;
			this->idleThreadsSize_++;
		}
	}
	
	return Result(task);
}

void ThreadPool::threadHandler(int threadId)
{
	std::chrono::time_point last =  std::chrono::system_clock::now();

	for(;;)
	{
		std::shared_ptr<Task> task;
		{
			std::cout << "Thread " << threadId << "获取任务!!!" << std::endl;
			// 加互斥锁
			std::unique_lock<std::mutex> lock(this->taskQueMtx);
			// 线程池停止运行
			while (currentTaskSize_ == 0)
			{
				if (!threadPoolIsRunning_)
				{
					std::cout << "Thread  " << threadId << "exit!!!";
					threadsMap_.erase(threadId);
					conExit_.notify_all();
					return;
				}

				// CACHED 模式下，如果一个线程空闲时间超过60s，将该线程进行收回
				if (this->mode_ == ThreadMode::CACHED) {
					// 一秒判断一下
					if (std::cv_status::timeout == taskQueNotEmpty.wait_for(lock, std::chrono::seconds(1)))
					{
						auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last);
						if (duration.count() > THREAD_IDLE_DURATION && currentThreadSize_ > this->initThreadSize_)
						{
							std::cout << "Thread  " << threadId << "exit!!!";
							threadsMap_.erase(threadId);  // 删除线程
							this->currentThreadSize_--;
							this->idleThreadsSize_--;
							// 返回
							return;
						}
					}
				}
				else
				{
					taskQueNotEmpty.wait(lock);
				}
			}

			// 获取第一个任务
			task = this->tasksQue_.front();
			this->tasksQue_.pop();
			this->currentTaskSize_--;
			this->idleThreadsSize_--;

			if(this->currentTaskSize_ > 0)
				taskQueNotEmpty.notify_all();

			// 继续通知线程可以进行提交生产任务
			taskQueNotFull.notify_all();
		}
		std::cout << "Thread " << threadId << "执行任务!!!" << std::endl;

		// 执行任务
		if (task != nullptr)
			task->exec();

		this->idleThreadsSize_++;
		last = std::chrono::system_clock::now();
	}
}

