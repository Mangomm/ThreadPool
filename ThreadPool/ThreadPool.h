#ifndef _ThreadPool_H
#define _ThreadPool_H

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

#define DEFAULT_TIME 10                 /* 10s���һ�� */
#define MIN_WAIT_TASK_NUM 10			/* ���queue_size > MIN_WAIT_TASK_NUM ����µ��̵߳��̳߳� */
#define DEFAULT_THREAD_VARY 10          /* ÿ�δ����������̵߳ĸ��� */

class CThreadPool;

typedef struct {
	void *(*function)(void *);          /* ����ָ�룬�ص����� */
	void *arg;                          /* ���溯���Ĳ��� */
} threadpool_task_t;                    /* �����߳�����ṹ�� */

struct CThreadItem {
	//Ĭ�Ϲ��캯��,�ṩ��CThreadPool��ʼ��m_threads��m_adjust
	CThreadItem() {}
	CThreadItem(CThreadPool *pool, std::thread *th, std::thread::id tid) :_pool(pool), _thread(th), _tid(tid), _ifrunning(false) {}

	//��������
	~CThreadItem() {}

	CThreadPool			*_pool;			//�̳߳�,��ʱδ��
	std::thread			*_thread;
	std::thread::id		_tid;
	bool			    _ifrunning;		//���߳��Ƿ��Ѿ�����,��ʱδ��
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
	std::vector<CThreadItem> m_threads;				/* ����̳߳���ÿ���̵߳�tid���� */
	//std::vector<CThreadItem*> m_garbage;			/* ����ȫ */
	std::vector<std::thread*> m_garbage;			/* ��ȫ����Ϊthread�������Լ�newȻ����vec�ĵ�ַ,���忴ReadMe��ע���4. */
	CThreadItem              m_adjust;				/* �����߳�,��Ϊʹ�ö������һ�����̶߳��� */

	std::mutex               m_lock;				/* ������ס���ṹ�� */
	std::mutex               m_busy_thread_counter;	/* ��¼æ״̬�̸߳���de�� -- busy_thr_num */
	std::mutex				 m_garbage_lock;		/* ������ס���ն��� */

	std::condition_variable m_queue_not_empty;		/* ���������Ϊ��ʱ�������ڸ�������������Ϊ��ʱ֪ͨ�̹߳��� */
	std::condition_variable m_queue_not_full;		/* �����������ʱ�����������߳������ڸ�������������Ϊ��ʱ�㲥֪ͨ�����ڴ������������߳̿���������� */

	threadpool_task_t *m_task_queue;				/* ������� */
	
public:
	int m_min_thr_num;								/* �̳߳���С�߳��� */
	int m_max_thr_num;								/* �̳߳�����߳��� */

	int m_queue_max_size;							/* task_queue���п��������������� */
	int m_queue_size;								/* task_queue����ʵ�������� */
	int m_queue_front;								/* task_queue��ͷ�±� */
	int m_queue_rear;								/* task_queue��β�±� */

	bool m_shutdown;								/* ��־λ���̳߳�ʹ��״̬��true��false */

	int m_busy_thr_num;								/* æ״̬�̸߳��� */
	int m_live_thr_num;					            /* ��ǰ����̸߳���,��threadpool_all_threadnum����,����˵��m_threads�Ĵ�С.����ͳһά��live���� */
	int m_wait_exit_thr_num;						/* Ҫ���ٵ��̸߳��� */

};

#endif