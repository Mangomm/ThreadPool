#include <iostream>
#include <thread>
#include "ThreadPool.h"

/* �̳߳��е��̣߳�ģ�⴦��ҵ�� */
void *process(void *arg)
{
	printf("thread working on task %d\n ", *(int *)arg);
	//sleep(1);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	printf("task %d is end\n", *(int *)arg);

	return NULL;
}


int main() {

	CThreadPool pool;													/*��C++ʹ��ʱ��һ����Ϊȫ�ֶ����
																		  ��global.h������Ȼ����main�����ж���*/


	//1 �ȴ����̳߳�
	pool.threadpool_create(3, 15, 100);									/*�����̳߳أ�������С3���̣߳����100���������100*/
	printf("pool inited");
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));//ȷ�������߳�������Ŀǰ���̳߳���δ��������������˯�ߴ���


	//2 ģ��ͻ������������50��
	//ע����������ʱnum�����СҲҪ�ı䣬������m_threads��joinable�ᱣ��δ֪���󣬵���������һ����~����
	int num[50], i;
	for (i = 0; i < 50; i++) {
		num[i] = i;
		printf("add task %d\n", i);
		pool.threadpool_add(process, (void*)&num[i]);					/* ���̳߳����������� */
	}

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