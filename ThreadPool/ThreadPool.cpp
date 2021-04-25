#include "ThreadPool.h"

void *adjust_thread(void *threadpool);

CThreadPool::CThreadPool() {

}

CThreadPool::~CThreadPool() {
	//threadpool_destroy();
}

//ok
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
				if (pool->m_queue_size != 0)			 //成后m_queue_size=0，所有线程都会阻塞在这里导致线程池卡死.所以虚假唤醒后，让它重新回到while中判断即可
					return true;
				return false;
			}*/);

			/*清除指定数目的空闲线程，如果要结束的线程个数大于0，结束线程*/
			if (pool->m_wait_exit_thr_num > 0) {
				pool->m_wait_exit_thr_num--;

				/*如果线程池里线程个数大于最小值时可以结束当前线程*/
				if (pool->m_live_thr_num > pool->m_min_thr_num) {
					for (auto it = pool->m_threads.begin(); it != pool->m_threads.end(); ) {
						if (it->_tid == std::this_thread::get_id()) {
							std::unique_lock<std::mutex> uGarbage(pool->m_garbage_lock);
							pool->m_garbage.emplace_back(it->_thread);
							uGarbage.unlock();
							printf("thread it->_tid=%d exiting\n", it->_tid);		//本线程tid，这里打印是安全的
							//printf("thread (*it)._tid=%d exiting\n", (*it)._tid);	//本线程tid，这里打印是安全的
							it = pool->m_threads.erase(it);							//删除后，要想继续使用迭代器，必须更新.注：这里的继续使用只能在下一次for循环及之后使用，而不能在本次for的erase后继续使用.具体解释看ReadMe的第3点
							//printf("thread tid=========%d exiting\n", it->_tid);	//不安全，不能在本次for的erase后继续使用迭代器it。
							break;
						}
						else {
							it++;
						}
					}
					pool->m_live_thr_num--;
					uLock.unlock();
					return NULL;//等价于pthread_exit(NULL);
				}
			}

		}

		/* 在执行任务之前，如果指定了true，要关闭线程池里的每个线程，自行退出处理，并且vec让destory处理即可. */
		if (pool->m_shutdown) {
			uLock.unlock();
			printf("thread %d exiting\n", std::this_thread::get_id());
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

//ok,后面优化一下即可
bool CThreadPool::threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
{
	if (min_thr_num <= 0
		|| min_thr_num > max_thr_num
		|| queue_max_size <= 0) 
	{
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

	m_live_thr_num = min_thr_num;

	/* 队列开辟空间 */
	this->m_task_queue = (threadpool_task_t *)new threadpool_task_t[sizeof(threadpool_task_t)*queue_max_size];
	if (this->m_task_queue == NULL) {
		printf("new task_queue fail.\n");
		return NULL;
	}

	//工作线程
	std::thread *th = NULL;
	for (int i = 0; i < min_thr_num; i++) {
		std::thread *th = (std::thread*)new std::thread(threadpool_thread, this);//threadpool_thread形参为指针，this不会进行拷贝构造
		if (th == NULL) {
			printf("new th fail.\n");
			threadpool_free();
			return NULL;
		}
		m_threads.emplace_back(this, th, th->get_id());
	}

	//管理线程
	th = (std::thread*)new std::thread(adjust_thread, this);
	if (th == NULL) {
		printf("new adjust_thread fail.\n");
		threadpool_free();
		return NULL;
	}
	this->m_adjust._pool = this;
	this->m_adjust._thread = th;
	this->m_adjust._tid = th->get_id();

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

//ok
int CThreadPool::threadpool_destroy()
{
	if (this->m_shutdown) {//已经销毁无须再次销毁
		return 0;
	}

	//std::unique_lock<std::mutex> uLock(this->m_lock);//destory时可以不上锁(因为我看别人已经使用稳定的线程池(自己也在使用)，在释放时也没加锁)，但vector会有影响吗--需要验证,最好放在析构处理，
	//这样就不需要考虑是否有影响，因为线程池此时无其它操作影响到join，实际上能影响回收时的vector的是，当任务数过大时，后面添加的管理线程将增加线程.但是这里先销毁管理线程就不会存在增加vec中线程的个数
	this->m_shutdown = true;//我们知道上锁是为了确定同步，使每次读取都有先后之分，但是这里线程退出时，可以不需要区分

	//先销毁管理线程
	if (this->m_adjust._thread->joinable()) {
		this->m_adjust._thread->join();//这里管理线程join,必定会等待adjust函数睡醒并执行完会while判断退出再回收.
	}
	if (this->m_adjust._thread != NULL) {
		delete this->m_adjust._thread;
		this->m_adjust._thread = NULL;
	}
	
	//回收工作线程
	threadpool_free();

	if (NULL != this->m_task_queue) {//回收任务队列
		delete this->m_task_queue;
		this->m_task_queue = NULL;
	}

	return 0;
}

//ok, 线程池结束回收vec
void CThreadPool::threadpool_free()
{
	//根据存活的线程，依次唤醒每一个线程
	for (int i = 0; i < this->m_live_thr_num; i++) {
		this->m_queue_not_empty.notify_all();//通知阻塞在没任务条件变量的所有线程
	}

	for (CThreadItem &item : this->m_threads) //依次等待各个子线程结束回收处理
	{
		if (item._thread->joinable()) {
			item._thread->join();//等待任务结束， 前提：线程一定会执行完
		}
		if (item._thread != NULL) {
			delete item._thread;
			item._thread = NULL;
		}
	}
	m_threads.clear();
}

//管理线程,ok
void *adjust_thread(void *threadpool)
{
	int i;
	CThreadPool *pool = (CThreadPool *)threadpool;
	if (pool == NULL) {
		std::cout << "pool is null in adjust_thread, pool end." << std::endl;
		return NULL;
	}

	while (pool->m_shutdown == false)
	{                                
		std::this_thread::sleep_for(std::chrono::seconds(DEFAULT_TIME));		/* 一定定时对线程池进行管理 */

		std::unique_lock<std::mutex> uLock(pool->m_lock);
		int queue_size = pool->m_queue_size;									/* 关注 任务数 */
		int live_thr_num = pool->m_live_thr_num;								/* 存活 线程数，这两个变量实际上是参考作用，实际判断时必须使用成员变量，因为这里释放锁后工作线程threadpool_thread拿
																				到锁可能后可能存在部分线程结束，导致成员变量与该变量值不一样 */
		//int live_thr_num = pool->m_threads.size();
		uLock.unlock();

		std::unique_lock<std::mutex> busyLock(pool->m_busy_thread_counter);
		int busy_thr_num = pool->m_busy_thr_num;                  /* 忙着的线程数 */
		busyLock.unlock();


		/* 创建新线程 算法： 任务数大于最小线程池个数, 且存活的线程数少于最大线程个数时 如：30>=10 && 40<100*/
		if (queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->m_max_thr_num) {
			uLock.lock();
			int add = 0;

			/* 一次增加 DEFAULT_THREAD_VARY 个线程 */
			for (i = 0; i < pool->m_max_thr_num && add < DEFAULT_THREAD_VARY && pool->m_live_thr_num < pool->m_max_thr_num; i++) {
				//这里并没去判断vec中可能存在死亡的线程(C语言可以使用信号).但是这个可以忽略, 
				//可以忽略3~5个线程死亡对程序造成的影响.传参时可以使m_max_thr_num稍微增大3~5个以弥补这个影响
				//并且线程开启后线程因系统原因结束的几率几乎是0，即使是被结束了，我们也可以自动调整线程数，所以无需判断。
				std::thread *th = (std::thread*)new std::thread(threadpool_thread, (void*)pool);
				if (th == NULL) {
					printf("incre thread faid, it will continue to incre.\n");
					continue;//若全部new失败，则增加0个线程
				}
				pool->m_threads.emplace_back(pool, th, th->get_id());
				printf("incre thread tid=%d.\n", th->get_id());
				add++;
				pool->m_live_thr_num++;
			}

			uLock.unlock();
		}

		/* 销毁多余的空闲线程 算法：忙线程x2 小于 存活的线程数 且 存活的线程数 大于 最小线程数时*/
		if ((busy_thr_num * 2) < live_thr_num  &&  live_thr_num > pool->m_min_thr_num) {

			/* 一次销毁DEFAULT_THREAD个线程, 随机10個即可 */
			uLock.lock();
			pool->m_wait_exit_thr_num = DEFAULT_THREAD_VARY;      /* 要销毁的线程数 设置为10 */
			uLock.unlock();

			for (i = 0; i < DEFAULT_THREAD_VARY; i++) {
				/* 通知处在空闲状态的线程, 他们会自行终止 */
				pool->m_queue_not_empty.notify_one();
			}
		}

		//清理本次因空闲而结束线程内存
		std::unique_lock<std::mutex> uGarbage(pool->m_garbage_lock);
		if (!pool->m_garbage.empty()) {
			for (auto it = pool->m_garbage.begin(); it != pool->m_garbage.end(); it++) 
			{
				if ((*it) != NULL) {
					if (((*it)->joinable())) {
						std::cout << "空闲线程内存被回收,tid=" << (*it)->get_id() << std::endl;
						(*it)->join();			//即使线程函数已经结束,也要join，否则报abort中断异常错误
						delete *it;
						*it = NULL;
					}
				}
			}
			pool->m_garbage.clear();//一次清除比多次erase效率快
		}
		uGarbage.unlock();

	}

	return NULL;
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

//ok，获取当前一开辟的总线程数,这里可能需要和很多线程去竞争m_lock,例如在add任务时需要，不过更多的是与threadpool_thread这个函数内部竞争
int CThreadPool::threadpool_all_threadnum()
{
	int allThrNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_lock);
	//allThrNum = m_threads.size();
	allThrNum = this->m_live_thr_num;
	uLock.unlock();

	return allThrNum;
}