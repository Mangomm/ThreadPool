#ifndef _ThreadPool_H
#define _ThreadPool_H

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>


typedef struct {
	void *(*function)(void *);          /* ����ָ�룬�ص����� */
	void *arg;                          /* ���溯���Ĳ��� */
} threadpool_task_t;                    /* �����߳�����ṹ�� */


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

	std::vector<std::thread> m_threads;				/* ����̳߳���ÿ���̵߳�tid������ */

	std::mutex m_lock;								/* ������ס���ṹ�� */
	std::mutex m_busy_thread_counter;				/* ��¼æ״̬�̸߳���de�� -- busy_thr_num */

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

};

#endif