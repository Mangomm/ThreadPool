#include <iostream>
#include <thread>
#include "ThreadPool.h"

/* 线程池中的线程，模拟处理业务 */
void *process(void *arg)
{
	printf("thread working on task %d\n ", *(int *)arg);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	printf("task %d is end\n", *(int *)arg);

	return NULL;
}


int main() {

	CThreadPool pool;													/*在C++使用时，一般作为全局对象放
																		  在global.h声明，然后在main函数中定义*/

	//1 先创建线程池
	pool.threadpool_create(5, 15, 100);									/*创建线程池，池里最小3个线程，最大100，队列最大100*/
	printf("pool inited");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));		//确保所有线程起来，目前在线程池暂未处理，现在先用睡眠处理,实际上由于我们有调整线程，可以忽略这一点


	//2 模拟客户端请求的任务50个
	//注意添加任务时num数组大小也要改变，否则导致m_threads在joinable会保存未知错误，导致我找了一晚上~日了
	int num[300], i;
	for (i = 0; i < 300; i++) {
		num[i] = i;
		printf("add task %d\n", i);
		pool.threadpool_add(process, (void*)&num[i]);					/* 向线程池中添加任务 */
	}

	int busyNum = pool.threadpool_busy_threadnum();
	std::cout << "BusyNum = " << busyNum << std::endl;
	int allThrNum = pool.threadpool_all_threadnum();
	std::cout << "allThrNum = " << allThrNum << std::endl;


	//3 等子线程完成任务
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));		/* 等子线程完成任务 */                         

	std::cout << "main sleep ok " << std::endl;

	//4 销毁线程池
	pool.threadpool_destroy();

	std::cout << "main end " << std::endl;

	return 0;
}