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

	/* �߳�ִ������ٴλص����while�ȵ���һ�ε����� */
	while (true)
	{
		threadpool_task_t task;
		std::unique_lock<std::mutex> uLock(pool->m_lock);//��ס���̳߳�

		while (pool->m_shutdown == false && pool->m_queue_size == 0) //�̳߳ؿ�������û����ʱ��������������
		{
			pool->m_queue_not_empty.wait(uLock/*, [&]() {//����C++11��д�̳߳�ʱ����2��lambda�������(ĳЩ���Ͽ��ܱ������)����������������
				if (pool->m_queue_size != 0)			 //�ɺ�m_queue_size=0�������̶߳������������ﵼ���̳߳ؿ���.������ٻ��Ѻ��������»ص�while���жϼ���
					return true;
				return false;
			}*/);

			/*���ָ����Ŀ�Ŀ����̣߳����Ҫ�������̸߳�������0�������߳�*/
			if (pool->m_wait_exit_thr_num > 0) {
				pool->m_wait_exit_thr_num--;

				/*����̳߳����̸߳���������Сֵʱ���Խ�����ǰ�߳�*/
				if (pool->m_live_thr_num > pool->m_min_thr_num) {
					for (auto it = pool->m_threads.begin(); it != pool->m_threads.end(); ) {
						if (it->_tid == std::this_thread::get_id()) {
							std::unique_lock<std::mutex> uGarbage(pool->m_garbage_lock);
							pool->m_garbage.emplace_back(it->_thread);
							uGarbage.unlock();
							printf("thread it->_tid=%d exiting\n", it->_tid);		//���߳�tid�������ӡ�ǰ�ȫ��
							//printf("thread (*it)._tid=%d exiting\n", (*it)._tid);	//���߳�tid�������ӡ�ǰ�ȫ��
							it = pool->m_threads.erase(it);							//ɾ����Ҫ�����ʹ�õ��������������.ע������ļ���ʹ��ֻ������һ��forѭ����֮��ʹ�ã��������ڱ���for��erase�����ʹ��.������Ϳ�ReadMe�ĵ�3��
							//printf("thread tid=========%d exiting\n", it->_tid);	//����ȫ�������ڱ���for��erase�����ʹ�õ�����it��
							break;
						}
						else {
							it++;
						}
					}
					pool->m_live_thr_num--;
					uLock.unlock();
					return NULL;//�ȼ���pthread_exit(NULL);
				}
			}

		}

		/* ��ִ������֮ǰ�����ָ����true��Ҫ�ر��̳߳����ÿ���̣߳������˳���������vec��destory������. */
		if (pool->m_shutdown) {
			uLock.unlock();
			printf("thread %d exiting\n", std::this_thread::get_id());
			//pthread_exit(NULL);			  /* �߳����н��� */
			return NULL;					  /*�൱��pthread_exit*/
		}

		//�˳�while��˵����������Ҫִ�У����Ҹ��߳��õ���m_lock

		/* ��ȡһ������ */
		task.function = pool->m_task_queue[pool->m_queue_front].function;
		task.arg = pool->m_task_queue[pool->m_queue_front].arg;
		/*���¶�ͷ*/
		pool->m_queue_front = (pool->m_queue_front + 1) % pool->m_queue_max_size;
		/*����������*/
		pool->m_queue_size--;

		/* �㲥֪ͨ�������µ�������ӽ��� */
		//pthread_cond_broadcast(&(pool->queue_not_full));
		pool->m_queue_not_full.notify_all();

		/* �������̳߳ؽṹ�� */
		uLock.unlock();

		//ͳ������ִ��������߳���
		std::unique_lock<std::mutex> busyThrNumLock(pool->m_busy_thread_counter);
		pool->m_busy_thr_num++;
		busyThrNumLock.unlock();

		(*(task.function))(task.arg);    /* ִ�лص���������,����д��task.function(task.arg); */

		busyThrNumLock.lock();
		pool->m_busy_thr_num--;
		busyThrNumLock.unlock();
	}

	return NULL;//�ȼ���pthread_exit(NULL);���Բ�Ҫ����Ϊ����û��break
}

//ok,�����Ż�һ�¼���
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
	m_queue_size = 0;			//Ŀǰ�����������Ϊ0
	m_queue_front = 0;			//��ʼѭ�����ж�ͷ��β��ָ��0
	m_queue_rear = 0; 

	m_shutdown = false;			//���ر��̳߳�
	m_busy_thr_num = 0;

	m_live_thr_num = min_thr_num;

	/* ���п��ٿռ� */
	this->m_task_queue = (threadpool_task_t *)new threadpool_task_t[sizeof(threadpool_task_t)*queue_max_size];
	if (this->m_task_queue == NULL) {
		printf("new task_queue fail.\n");
		return NULL;
	}

	//�����߳�
	std::thread *th = NULL;
	for (int i = 0; i < min_thr_num; i++) {
		std::thread *th = (std::thread*)new std::thread(threadpool_thread, this);//threadpool_thread�β�Ϊָ�룬this������п�������
		if (th == NULL) {
			printf("new th fail.\n");
			threadpool_free();
			return NULL;
		}
		m_threads.emplace_back(this, th, th->get_id());
	}

	//�����߳�
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

//ok,������������ӹ��ܴ���
int CThreadPool::threadpool_add(void*(*function)(void *arg), void *arg)
{
	//�ж������Ƿ�����
	std::unique_lock<std::mutex> uLock(this->m_lock);

	while (this->m_queue_size >= this->m_queue_max_size && this->m_shutdown == false) 
	{
		//����Ϊ������һֱ������m_queue_not_full��������
		this->m_queue_not_full.wait(uLock, [this]() {
			if (this->m_queue_size < this->m_queue_max_size)
				return true;
			return false;
		});
	}

	/* ��Ϊ�����������Ϊ�̳߳���ֹ���˳������Կ���ͨ������������m_queue_not_empty�������߳�ʹ���Ƕ��˳� */
	if (this->m_shutdown) {
		this->m_queue_not_empty.notify_all();
		uLock.unlock();
		return 0;//�����notify_all��return���Բ�д�����Խ�������Ĳ�������������û���д
	}

	/* ��չ����̵߳Ļص������Ͳ���arg,�о���һ����̫��Ҫ������������� */
	if (this->m_task_queue[this->m_queue_rear].arg != NULL) {
		this->m_task_queue[this->m_queue_rear].arg = NULL;
	}
	if (this->m_task_queue[this->m_queue_rear].function != NULL) {
		this->m_task_queue[this->m_queue_rear].function = NULL;
	}
	
	//��ʱ˵�������������,�����õ�����Ȼ����Ϊ������while�������˳�ѭ��
	this->m_task_queue[this->m_queue_rear].function = function;
	this->m_task_queue[this->m_queue_rear].arg = arg;
	//���¶�β
	this->m_queue_rear = (this->m_queue_rear + 1) % this->m_queue_max_size;
	//��������1
	this->m_queue_size += 1;

	//֪ͨ������m_queue_not_empty���̣߳�˵������������Ҫִ��
	m_queue_not_empty.notify_one();//����дall������дone����ʹЧ�ʸ���

	uLock.unlock();//����ʡ��

	return 0;
}

//ok
int CThreadPool::threadpool_destroy()
{
	if (this->m_shutdown) {//�Ѿ����������ٴ�����
		return 0;
	}

	//std::unique_lock<std::mutex> uLock(this->m_lock);//destoryʱ���Բ�����(��Ϊ�ҿ������Ѿ�ʹ���ȶ����̳߳�(�Լ�Ҳ��ʹ��)�����ͷ�ʱҲû����)����vector����Ӱ����--��Ҫ��֤,��÷�����������
	//�����Ͳ���Ҫ�����Ƿ���Ӱ�죬��Ϊ�̳߳ش�ʱ����������Ӱ�쵽join��ʵ������Ӱ�����ʱ��vector���ǣ�������������ʱ��������ӵĹ����߳̽������߳�.�������������ٹ����߳̾Ͳ����������vec���̵߳ĸ���
	this->m_shutdown = true;//����֪��������Ϊ��ȷ��ͬ����ʹÿ�ζ�ȡ�����Ⱥ�֮�֣����������߳��˳�ʱ�����Բ���Ҫ����

	//�����ٹ����߳�
	if (this->m_adjust._thread->joinable()) {
		this->m_adjust._thread->join();//��������߳�join,�ض���ȴ�adjust����˯�Ѳ�ִ�����while�ж��˳��ٻ���.
	}
	if (this->m_adjust._thread != NULL) {
		delete this->m_adjust._thread;
		this->m_adjust._thread = NULL;
	}
	
	//���չ����߳�
	threadpool_free();

	if (NULL != this->m_task_queue) {//�����������
		delete this->m_task_queue;
		this->m_task_queue = NULL;
	}

	return 0;
}

//ok, �̳߳ؽ�������vec
void CThreadPool::threadpool_free()
{
	//���ݴ����̣߳����λ���ÿһ���߳�
	for (int i = 0; i < this->m_live_thr_num; i++) {
		this->m_queue_not_empty.notify_all();//֪ͨ������û�������������������߳�
	}

	for (CThreadItem &item : this->m_threads) //���εȴ��������߳̽������մ���
	{
		if (item._thread->joinable()) {
			item._thread->join();//�ȴ���������� ǰ�᣺�߳�һ����ִ����
		}
		if (item._thread != NULL) {
			delete item._thread;
			item._thread = NULL;
		}
	}
	m_threads.clear();
}

//�����߳�,ok
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
		std::this_thread::sleep_for(std::chrono::seconds(DEFAULT_TIME));		/* һ����ʱ���̳߳ؽ��й��� */

		std::unique_lock<std::mutex> uLock(pool->m_lock);
		int queue_size = pool->m_queue_size;									/* ��ע ������ */
		int live_thr_num = pool->m_live_thr_num;								/* ��� �߳���������������ʵ�����ǲο����ã�ʵ���ж�ʱ����ʹ�ó�Ա��������Ϊ�����ͷ��������߳�threadpool_thread��
																				�������ܺ���ܴ��ڲ����߳̽��������³�Ա������ñ���ֵ��һ�� */
		//int live_thr_num = pool->m_threads.size();
		uLock.unlock();

		std::unique_lock<std::mutex> busyLock(pool->m_busy_thread_counter);
		int busy_thr_num = pool->m_busy_thr_num;                  /* æ�ŵ��߳��� */
		busyLock.unlock();


		/* �������߳� �㷨�� ������������С�̳߳ظ���, �Ҵ����߳�����������̸߳���ʱ �磺30>=10 && 40<100*/
		if (queue_size >= MIN_WAIT_TASK_NUM && live_thr_num < pool->m_max_thr_num) {
			uLock.lock();
			int add = 0;

			/* һ������ DEFAULT_THREAD_VARY ���߳� */
			for (i = 0; i < pool->m_max_thr_num && add < DEFAULT_THREAD_VARY && pool->m_live_thr_num < pool->m_max_thr_num; i++) {
				//���ﲢûȥ�ж�vec�п��ܴ����������߳�(C���Կ���ʹ���ź�).����������Ժ���, 
				//���Ժ���3~5���߳������Գ�����ɵ�Ӱ��.����ʱ����ʹm_max_thr_num��΢����3~5�����ֲ����Ӱ��
				//�����߳̿������߳���ϵͳԭ������ļ��ʼ�����0����ʹ�Ǳ������ˣ�����Ҳ�����Զ������߳��������������жϡ�
				std::thread *th = (std::thread*)new std::thread(threadpool_thread, (void*)pool);
				if (th == NULL) {
					printf("incre thread faid, it will continue to incre.\n");
					continue;//��ȫ��newʧ�ܣ�������0���߳�
				}
				pool->m_threads.emplace_back(pool, th, th->get_id());
				printf("incre thread tid=%d.\n", th->get_id());
				add++;
				pool->m_live_thr_num++;
			}

			uLock.unlock();
		}

		/* ���ٶ���Ŀ����߳� �㷨��æ�߳�x2 С�� �����߳��� �� �����߳��� ���� ��С�߳���ʱ*/
		if ((busy_thr_num * 2) < live_thr_num  &&  live_thr_num > pool->m_min_thr_num) {

			/* һ������DEFAULT_THREAD���߳�, ���10������ */
			uLock.lock();
			pool->m_wait_exit_thr_num = DEFAULT_THREAD_VARY;      /* Ҫ���ٵ��߳��� ����Ϊ10 */
			uLock.unlock();

			for (i = 0; i < DEFAULT_THREAD_VARY; i++) {
				/* ֪ͨ���ڿ���״̬���߳�, ���ǻ�������ֹ */
				pool->m_queue_not_empty.notify_one();
			}
		}

		//����������ж������߳��ڴ�
		std::unique_lock<std::mutex> uGarbage(pool->m_garbage_lock);
		if (!pool->m_garbage.empty()) {
			for (auto it = pool->m_garbage.begin(); it != pool->m_garbage.end(); it++) 
			{
				if ((*it) != NULL) {
					if (((*it)->joinable())) {
						std::cout << "�����߳��ڴ汻����,tid=" << (*it)->get_id() << std::endl;
						(*it)->join();			//��ʹ�̺߳����Ѿ�����,ҲҪjoin������abort�ж��쳣����
						delete *it;
						*it = NULL;
					}
				}
			}
			pool->m_garbage.clear();//һ������ȶ��eraseЧ�ʿ�
		}
		uGarbage.unlock();

	}

	return NULL;
}

//ok����ȡ����ִ��������߳���
int CThreadPool::threadpool_busy_threadnum()
{
	int busyNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_busy_thread_counter);
	busyNum = this->m_busy_thr_num;
	uLock.unlock();

	return busyNum;
}

//ok����ȡ��ǰһ���ٵ����߳���,���������Ҫ�ͺܶ��߳�ȥ����m_lock,������add����ʱ��Ҫ���������������threadpool_thread��������ڲ�����
int CThreadPool::threadpool_all_threadnum()
{
	int allThrNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_lock);
	//allThrNum = m_threads.size();
	allThrNum = this->m_live_thr_num;
	uLock.unlock();

	return allThrNum;
}