#include "threadpool.h"

class MyTask : public Task
{

private:
	int start_;
	int end_;

public:
	MyTask(int start, int end) : start_(start), end_(end) {}
	Any run()
	{
		uLong ret = 0;
		for (int i = start_; i <= end_; i++)
		{
			ret += i;
		}
		// std::this_thread::sleep_for(std::chrono::seconds(3));

		return ret;
	}
};

int main(int agrc, char **argv)
{
	clock_t starttime = clock();
	ThreadPool pool;
	pool.setMode(ThreadMode::CACHED);
	pool.start(3);
	Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.submitTask(std::make_shared<MyTask>(300000001, 400000000));
	pool.submitTask(std::make_shared<MyTask>(400000001, 500000000));

	std::cout << res1.get().cast_<uLong>() + res2.get().cast_<uLong>() + res3.get().cast_<uLong>() << std::endl;
	std::cout << "�̳߳ػ��ѵ�ʱ�� :" << clock() - starttime << "ms" << std::endl;
	clock_t nonPoolThreadStartTime = clock();
	uLong sum = 0;
	for (uLong i = 1; i <= 300000000; i++)
		sum += i;

	std::cout << "���̳߳ػ��ѵ�ʱ�� :" << clock() - nonPoolThreadStartTime << "ms" << std::endl;

	std::cout << sum << std::endl;

	char c = getchar();

	return 0;
}