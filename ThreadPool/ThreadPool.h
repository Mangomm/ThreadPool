#ifndef _ThreadPool_H
#define _ThreadPool_H

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#define DEFAULT_TIME 10                 /* 10s检测一次 */
#define MIN_WAIT_TASK_NUM 10			/* 如果queue_size > MIN_WAIT_TASK_NUM 添加新的线程到线程池 */
#define DEFAULT_THREAD_VARY 10          /* 每次创建和销毁线程的个数 */

class CThreadPool;

typedef struct {
	void *(*function)(void *);          /* 函数指针，回调函数 */
	void *arg;                          /* 上面函数的参数 */
} threadpool_task_t;                    /* 各子线程任务结构体 */

struct CThreadItem {
	//默认构造函数,提供给CThreadPool初始化m_threads，m_adjust
	CThreadItem() {}
	CThreadItem(CThreadPool *pool, std::thread *th, std::thread::id tid) :_pool(pool), _thread(th), _tid(tid), _ifrunning(false) {}

	//析构函数
	~CThreadItem() {}

	CThreadPool			*_pool;			//线程池,暂时未用
	std::thread			*_thread;
	std::thread::id		_tid;
	bool			    _ifrunning;		//本线程是否已经开启,暂时未用
};

class CThreadPool {
public:
	CThreadPool();
	virtual ~CThreadPool();
	bool    threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
	int     threadpool_add(void*(*function)(void *arg), void *arg);
	int     threadpool_destroy();
	void    threadpool_free();

	int     threadpool_busy_threadnum();
	int     threadpool_all_threadnum();

public:
	std::vector<CThreadItem> m_threads;				/* 存放线程池中每个线程的tid数组 */
	//std::vector<CThreadItem*> m_garbage;			/* 不安全 */
	std::vector<std::thread*> m_garbage;			/* 安全，因为thread是我们自己new然后传入vec的地址,具体看ReadMe的注意点4. */
	CThreadItem              m_adjust;				/* 管理线程,因为使用对象会是一个非线程对象 */

	std::mutex               m_lock;				/* 用于锁住本结构体 */
	std::mutex               m_busy_thread_counter;	/* 记录忙状态线程个数de琐 -- busy_thr_num */
	std::mutex				 m_garbage_lock;		/* 用于锁住回收队列 */

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
	int m_live_thr_num;					            /* 当前存活线程个数,即threadpool_all_threadnum个数,或者说是m_threads的大小.这里统一维护live更好 */
	int m_wait_exit_thr_num;						/* 要销毁的线程个数 */

};

#endif