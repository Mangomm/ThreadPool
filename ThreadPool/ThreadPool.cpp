#include "ThreadPool.h"


CThreadPool::CThreadPool() {

}

CThreadPool::~CThreadPool() {
	//threadpool_destroy();
}

//ok,后面需要额外添加管理线程的功能代码
void *threadpool_thread(void *threadpool) {

	CThreadPool *pool = (CThreadPool*)threadpool;

	/* 线程执行完毕再次回到外层while等到下一次的任务 */
	while (true)
	{
		threadpool_task_t task;
		std::unique_lock<std::mutex> uLock(pool->m_lock);//锁住本线程池

		while (pool->m_shutdown == false && pool->m_queue_size == 0) //线程池开启并且没任务时阻塞在条件变量
		{
			pool->m_queue_not_empty.wait(uLock/*, [&]() {//在用C++11编写线程池时，参2的lambda不能添加(某些场合可能必须添加)，否则当所有任务完
				if (pool->m_queue_size != 0)		   //成后m_queue_size=0，所有线程都会阻塞在这里导致线程池卡死.所以虚假唤醒后，让它重新回到while中判断即可
					return true;
				return false;
			}*/);
		}

		/* 在执行任务之前，如果指定了true，要关闭线程池里的每个线程，自行退出处理 */
		if (pool->m_shutdown) {
			uLock.unlock();
			printf("thread is exiting\n");
			//pthread_exit(NULL);			  /* 线程自行结束 */
			return NULL;					  /*相当于pthread_exit*/
		}

		//退出while，说明有任务需要执行，并且该线程拿到了m_lock

		/* 获取一个任务 */
		task.function = pool->m_task_queue[pool->m_queue_front].function;
		task.arg = pool->m_task_queue[pool->m_queue_front].arg;
		/*更新队头*/
		pool->m_queue_front = (pool->m_queue_front + 1) % pool->m_queue_max_size;
		/*更新任务数*/
		pool->m_queue_size--;

		/* 广播通知可以有新的任务添加进来 */
		//pthread_cond_broadcast(&(pool->queue_not_full));
		pool->m_queue_not_full.notify_all();

		/* 解锁本线程池结构体 */
		uLock.unlock();

		//统计正在执行任务的线程数
		std::unique_lock<std::mutex> busyThrNumLock(pool->m_busy_thread_counter);
		pool->m_busy_thr_num++;
		busyThrNumLock.unlock();

		(*(task.function))(task.arg);    /* 执行回调函数任务,或者写成task.function(task.arg); */

		busyThrNumLock.lock();
		pool->m_busy_thr_num--;
		busyThrNumLock.unlock();
	}

	return NULL;//等价于pthread_exit(NULL);可以不要，因为上面没有break
}

//ok,后面需要额外添加管理线程的功能代码
bool CThreadPool::threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
	if (min_thr_num <= 0
		|| min_thr_num > max_thr_num
		|| queue_max_size <= 0) {
		return false;
	}

	m_min_thr_num = min_thr_num;
	m_max_thr_num = max_thr_num;

	m_queue_max_size = queue_max_size;
	m_queue_size = 0;			//目前队列任务个数为0
	m_queue_front = 0;			//开始循环队列队头队尾均指向0
	m_queue_rear = 0; 

	m_shutdown = false;			//不关闭线程池
	m_busy_thr_num = 0;

	/* 队列开辟空间 */
	this->m_task_queue = (threadpool_task_t *)new threadpool_task_t[sizeof(threadpool_task_t)*queue_max_size];
	if (this->m_task_queue == NULL) {
		printf("new task_queue fail");
		return NULL;
	}

	for (int i = 0; i < min_thr_num; i++) {
		m_threads.emplace_back(threadpool_thread, this);
	}

	return true;
}

//ok,后面无需再添加功能代码
int CThreadPool::threadpool_add(void*(*function)(void *arg), void *arg)
{
	//判断任务是否已满
	std::unique_lock<std::mutex> uLock(this->m_lock);

	while (this->m_queue_size >= this->m_queue_max_size && this->m_shutdown == false) 
	{
		//任务为满，则一直阻塞在m_queue_not_full条件变量
		this->m_queue_not_full.wait(uLock, [this]() {
			if (this->m_queue_size < this->m_queue_max_size)
				return true;
			return false;
		});
	}

	/* 因为上面可能是因为线程池终止而退出，所以可以通过唤醒阻塞在m_queue_not_empty的所有线程使它们都退出 */
	if (this->m_shutdown) {
		this->m_queue_not_empty.notify_all();
		uLock.unlock();
		return 0;//这里的notify_all和return可以不写，可以借助下面的操作处理，不过最好还是写
	}

	/* 清空工作线程的回调函数和参数arg,感觉这一步不太需要，不过最好清理 */
	if (this->m_task_queue[this->m_queue_rear].arg != NULL) {
		this->m_task_queue[this->m_queue_rear].arg = NULL;
	}
	if (this->m_task_queue[this->m_queue_rear].function != NULL) {
		this->m_task_queue[this->m_queue_rear].function = NULL;
	}
	
	//此时说明可以添加任务,并且拿到锁，然后因为不满足while条件而退出循环
	this->m_task_queue[this->m_queue_rear].function = function;
	this->m_task_queue[this->m_queue_rear].arg = arg;
	//更新队尾
	this->m_queue_rear = (this->m_queue_rear + 1) % this->m_queue_max_size;
	//任务数加1
	this->m_queue_size += 1;

	//通知阻塞在m_queue_not_empty的线程，说有新任务到来需要执行
	m_queue_not_empty.notify_one();//上面写all，这里写one，能使效率更高

	uLock.unlock();//可以省略

	return 0;
}

//ok,后面需要额外添加管理线程的功能代码
int CThreadPool::threadpool_destroy()
{
	if (this->m_shutdown) {//已经销毁无须再次销毁
		return 0;
	}

	//std::unique_lock<std::mutex> uLock(this->m_lock);//destory时可以不上锁(因为我看别人已经使用稳定的线程池(自己也在使用)，在释放时也没加锁)，但vector会有影响吗--需要验证,最好放在析构处理，
	//这样就不需要考虑是否有影响，因为线程池此时无其它操作影响到join，实际上能影响回收时的vector的是，当任务数过大时，后面添加的管理线程将增加线程.所以此时根本对vector没影响，完全不需要加锁
	this->m_shutdown = true;//我们知道上锁是为了确定同步，使每次读取都有先后之分，但是这里线程退出时，可以不需要区分

	this->m_queue_not_empty.notify_all();//通知阻塞在没任务条件变量的所有线程

	for (std::thread &th : this->m_threads) //依次等待各个子线程结束回收处理
	{
		if (th.joinable()) {
			th.join();//等待任务结束， 前提：线程一定会执行完
		}
	}
	
	if (NULL != this->m_task_queue) {//回收任务队列
		delete this->m_task_queue;
		this->m_task_queue = NULL;
	}

	return 0;
}

//ok，获取正在执行任务的线程数
int CThreadPool::threadpool_busy_threadnum()
{
	int busyNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_busy_thread_counter);
	busyNum = this->m_busy_thr_num;
	uLock.unlock();

	return busyNum;
}

//获取当前一开辟的总线程数,这里可能需要和很多线程去竞争m_lock,例如在add任务时需要，不过更多的是与threadpool_thread这个函数内部竞争
int CThreadPool::threadpool_all_threadnum()
{
	int allThrNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_lock);
	allThrNum = m_threads.size();
	uLock.unlock();

	return allThrNum;
}