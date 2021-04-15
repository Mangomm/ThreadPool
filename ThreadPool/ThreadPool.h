#ifndef _ThreadPool_H
#define _ThreadPool_H

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>


typedef struct {
	void *(*function)(void *);          /* 函数指针，回调函数 */
	void *arg;                          /* 上面函数的参数 */
} threadpool_task_t;                    /* 各子线程任务结构体 */


class CThreadPool {
public:
	CThreadPool();
	virtual ~CThreadPool();
	bool threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
	int threadpool_add(void*(*function)(void *arg), void *arg);
	int threadpool_destroy();

	int threadpool_busy_threadnum();
	int threadpool_all_threadnum();
public:

	std::vector<std::thread> m_threads;				/* 存放线程池中每个线程的tid。数组 */

	std::mutex m_lock;								/* 用于锁住本结构体 */
	std::mutex m_busy_thread_counter;				/* 记录忙状态线程个数de琐 -- busy_thr_num */

	std::condition_variable m_queue_not_empty;		/* 任务队列里为空时，阻塞在该条件变量，不为空时通知线程工作 */
	std::condition_variable m_queue_not_full;		/* 当任务队列满时，添加任务的线程阻塞在该条件变量，不为满时广播通知阻塞在此条件变量的线程可以添加任务 */

	threadpool_task_t *m_task_queue;				/* 任务队列 */
	
	
public:
	int m_min_thr_num;								/* 线程池最小线程数 */
	int m_max_thr_num;								/* 线程池最大线程数 */

	int m_queue_max_size;							/* task_queue队列可容纳任务数上限 */
	int m_queue_size;								/* task_queue队中实际任务数 */
	int m_queue_front;								/* task_queue队头下标 */
	int m_queue_rear;								/* task_queue队尾下标 */

	bool m_shutdown;								/* 标志位，线程池使用状态，true或false */

	int m_busy_thr_num;								/* 忙状态线程个数 */

};

#endif