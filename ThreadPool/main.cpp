#include <iostream>
#include <thread>
#include "ThreadPool.h"

/* �̳߳��е��̣߳�ģ�⴦��ҵ�� */
void *process(void *arg)
{
	printf("thread working on task %d\n ", *(int *)arg);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	printf("task %d is end\n", *(int *)arg);

	return NULL;
}


int main() {

	CThreadPool pool;													/*��C++ʹ��ʱ��һ����Ϊȫ�ֶ����
																		  ��global.h������Ȼ����main�����ж���*/

	//1 �ȴ����̳߳�
	pool.threadpool_create(5, 15, 100);									/*�����̳߳أ�������С3���̣߳����100���������100*/
	printf("pool inited");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));		//ȷ�������߳�������Ŀǰ���̳߳���δ������������˯�ߴ���,ʵ�������������е����̣߳����Ժ�����һ��


	//2 ģ��ͻ������������50��
	//ע���������ʱnum�����СҲҪ�ı䣬������m_threads��joinable�ᱣ��δ֪���󣬵���������һ����~����
	int num[300], i;
	int r;
	srand(time(NULL));
Task:
	r = rand() % 300;
	for (i = 0; i < r; i++) {
		num[i] = i;
		printf("add task %d\n", i);
		pool.threadpool_add(process, (void*)&num[i]);					/* ���̳߳���������� */
	}

	//˫����д�С���󷨣�(q->rear - q->front + MAXSIZE) % MAXSIZE;
	printf("�����һ������,������=%d, ��0 ~ %d\n", r, r - 1);
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));	//ģ���̳߳�һֱ������.���԰��Сʱ���ң��ȶ���������ȷ
	//std::this_thread::sleep_for(std::chrono::milliseconds(15000));	//ģ���̳߳�����Ϊ�ջ��߲�Ϊ�յ����.���ӷ�����Թ����̵߳Ĵ���.���԰��Сʱ���ң��ȶ���������ȷ
	goto Task;


	int busyNum = pool.threadpool_busy_threadnum();
	std::cout << "BusyNum = " << busyNum << std::endl;
	int allThrNum = pool.threadpool_all_threadnum();
	std::cout << "allThrNum = " << allThrNum << std::endl;

	//3 �����߳��������
	std::this_thread::sleep_for(std::chrono::milliseconds(10000));		/* �����߳�������� */                         

	std::cout << "main sleep ok " << std::endl;

	//4 �����̳߳�
	pool.threadpool_destroy();

	std::cout << "main end " << std::endl;

	return 0;
}