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
Task::Task() : res_(nullptr)
{
}
Task::~Task()
{
}
void Task::setResult(Result *res)
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

ThreadPool::ThreadPool() : threadPoolIsRunning_(false),
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
	conExit_.wait(lock, [&]() -> bool
				  { return threadsMap_.size() == 0; });
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
	if (!checkThreadPoolIsRunning())
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
	// ??????????
	this->initThreadSize_ = threadSize;
	this->currentThreadSize_ = threadSize;

	// ??????????initThreadSize ??Thread???
	for (int i = 0; i < this->currentThreadSize_; i++)
	{
		// ?????????? std::unique_ptr
		auto f = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);
		// ???????????Thread??
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);
		// emplace_back????
		this->threadsMap_.emplace(ptr->getId(), std::move(ptr));
	}
	// ???????
	for (int i = 0; i < this->currentThreadSize_; i++)
	{
		this->threadsMap_[i]->start();
		this->idleThreadsSize_++;
	}
}

// ??????
Result ThreadPool::submitTask(std::shared_ptr<Task> task)
{
	std::cout << "????????" << std::endl;
	// ?????????
	std::unique_lock<std::mutex> lock(this->taskQueMtx);

	// ?????????§Ø?
	if (!this->taskQueNotFull.wait_for(lock, std::chrono::seconds(1), [&]() -> bool
									   { return (this->currentTaskSize_ < this->taskQueSizeThreshold_); }))
	{
		std::cerr << "???????????????" << std::endl;
		return Result(task, false);
	}

	this->tasksQue_.emplace(task);
	this->currentTaskSize_++;
	this->taskQueNotEmpty.notify_all();

	// CACHED??????????????§³???????????????????????§³???????????????????????
	if (mode_ == ThreadMode::CACHED)
	{
		if (this->currentTaskSize_ > this->idleThreadsSize_ &&
			this->currentThreadSize_ < this->threadsQueSizeThreshold_)
		{
			// ?????
			auto f = std::bind(&ThreadPool::threadHandler, this, std::placeholders::_1);
			// ???? std::unique_ptr??????????????????Thread????????
			std::unique_ptr<Thread> ptr = std::make_unique<Thread>(f);
			int id = ptr->getId();
			std::cout << "Thread " << id << "????" << std::endl;
			// ?????????thread?????????????????????
			this->threadsMap_.emplace(id, std::move(ptr));
			// ?????????????
			this->threadsMap_[id]->start();
			this->currentThreadSize_++;
			this->idleThreadsSize_++;
		}
	}

	return Result(task);
}

void ThreadPool::threadHandler(int threadId)
{
	// std::chrono::time_point last = std::chrono::system_clock::now();
	auto last = std::chrono::system_clock::now();

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			std::cout << "Thread " << threadId << "!!!" << std::endl;
			// ???????
			std::unique_lock<std::mutex> lock(this->taskQueMtx);
			// ??????????
			while (currentTaskSize_ == 0)
			{
				if (!threadPoolIsRunning_)
				{
					std::cout << "Thread  " << threadId << "exit!!!";
					threadsMap_.erase(threadId);
					conExit_.notify_all();
					return;
				}

				// CACHED ??????????????????????60s???????????????
				if (this->mode_ == ThreadMode::CACHED)
				{
					// ????§Ø????
					if (std::cv_status::timeout == taskQueNotEmpty.wait_for(lock, std::chrono::seconds(1)))
					{
						auto duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - last);
						if (duration.count() > THREAD_IDLE_DURATION && currentThreadSize_ > this->initThreadSize_)
						{
							std::cout << "Thread  " << threadId << "exit!!!";
							threadsMap_.erase(threadId); // ??????
							this->currentThreadSize_--;
							this->idleThreadsSize_--;
							// ????
							return;
						}
					}
				}
				else
				{
					taskQueNotEmpty.wait(lock);
				}
			}

			// ????????????
			task = this->tasksQue_.front();
			this->tasksQue_.pop();
			this->currentTaskSize_--;
			this->idleThreadsSize_--;

			if (this->currentTaskSize_ > 0)
				taskQueNotEmpty.notify_all();

			// ?????????????????????????
			taskQueNotFull.notify_all();
		}
		std::cout << "Thread " << threadId << "!!!" << std::endl;

		// ???????
		if (task != nullptr)
			task->exec();

		this->idleThreadsSize_++;
		last = std::chrono::system_clock::now();
	}
}
