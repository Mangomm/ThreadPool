#include "ThreadPool.h"


CThreadPool::CThreadPool() {

}

CThreadPool::~CThreadPool() {
	//threadpool_destroy();
}

//ok,������Ҫ������ӹ����̵߳Ĺ��ܴ���
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
				if (pool->m_queue_size != 0)		   //�ɺ�m_queue_size=0�������̶߳������������ﵼ���̳߳ؿ���.������ٻ��Ѻ��������»ص�while���жϼ���
					return true;
				return false;
			}*/);
		}

		/* ��ִ������֮ǰ�����ָ����true��Ҫ�ر��̳߳����ÿ���̣߳������˳����� */
		if (pool->m_shutdown) {
			uLock.unlock();
			printf("thread is exiting\n");
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

//ok,������Ҫ������ӹ����̵߳Ĺ��ܴ���
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
	m_queue_size = 0;			//Ŀǰ�����������Ϊ0
	m_queue_front = 0;			//��ʼѭ�����ж�ͷ��β��ָ��0
	m_queue_rear = 0; 

	m_shutdown = false;			//���ر��̳߳�
	m_busy_thr_num = 0;

	/* ���п��ٿռ� */
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

//ok,������Ҫ������ӹ����̵߳Ĺ��ܴ���
int CThreadPool::threadpool_destroy()
{
	if (this->m_shutdown) {//�Ѿ����������ٴ�����
		return 0;
	}

	//std::unique_lock<std::mutex> uLock(this->m_lock);//destoryʱ���Բ�����(��Ϊ�ҿ������Ѿ�ʹ���ȶ����̳߳�(�Լ�Ҳ��ʹ��)�����ͷ�ʱҲû����)����vector����Ӱ����--��Ҫ��֤,��÷�����������
	//�����Ͳ���Ҫ�����Ƿ���Ӱ�죬��Ϊ�̳߳ش�ʱ����������Ӱ�쵽join��ʵ������Ӱ�����ʱ��vector���ǣ�������������ʱ��������ӵĹ����߳̽������߳�.���Դ�ʱ������vectorûӰ�죬��ȫ����Ҫ����
	this->m_shutdown = true;//����֪��������Ϊ��ȷ��ͬ����ʹÿ�ζ�ȡ�����Ⱥ�֮�֣����������߳��˳�ʱ�����Բ���Ҫ����

	this->m_queue_not_empty.notify_all();//֪ͨ������û�������������������߳�

	for (std::thread &th : this->m_threads) //���εȴ��������߳̽������մ���
	{
		if (th.joinable()) {
			th.join();//�ȴ���������� ǰ�᣺�߳�һ����ִ����
		}
	}
	
	if (NULL != this->m_task_queue) {//�����������
		delete this->m_task_queue;
		this->m_task_queue = NULL;
	}

	return 0;
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

//��ȡ��ǰһ���ٵ����߳���,���������Ҫ�ͺܶ��߳�ȥ����m_lock,������add����ʱ��Ҫ���������������threadpool_thread��������ڲ�����
int CThreadPool::threadpool_all_threadnum()
{
	int allThrNum = -1;
	std::unique_lock<std::mutex> uLock(this->m_lock);
	allThrNum = m_threads.size();
	uLock.unlock();

	return allThrNum;
}