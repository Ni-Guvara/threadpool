#include "threadpool.h"
#include <iostream>
#include <functional>
#include <thread>
#include <future>
#include <chrono>

int sum1(int a, int b)
{
	return a + b;
}

int sum2(int a, int b, int c)
{
	return a + b + c;
}

int main(int agrc, char **argv)
{
	clock_t starttime = clock();

	ThreadPool pool;
	pool.start(2);

	future<int> num1 = pool.submitTask(sum1, 10, 20);
	future<int> num2 = pool.submitTask(sum2, 10, 20, 30);

	cout << num1.get() << endl;
	cout << num2.get() << endl;
	return 0;
}
