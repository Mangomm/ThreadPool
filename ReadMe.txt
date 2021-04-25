编写线程池时遇到的问题：
1）threadpool_thread中，为空时的条件变量.wait时，不能添加参2lambda表达式。否则会造成卡死。
2）模拟客户端请求的任务20个,注意添加任务时num数组也要添加，否则导致m_threads在joinable会保存未知错误，导致我找了一晚上~shift。

3）看代码：
/*如果线程池里线程个数大于最小值时可以结束当前线程*/
if (pool->m_live_thr_num > pool->m_min_thr_num) {
	printf("thread is prepare exiting\n");
	for (auto it = pool->m_threads.begin(); it != pool->m_threads.end(); ) {
		if (it->_tid == std::this_thread::get_id()) {
			printf("thread ==========%d exiting\n", it->_tid);	//本线程tid，这里打印是安全的
			//printf("thread tid=%d exiting\n", (*it)._tid);	//本线程tid，这里打印是安全的
			it = pool->m_threads.erase(it);						//删除后，要想继续使用迭代器，必须更新.注：这里的继续使用只能在下一次for循环及之后使用，而不能在本次for的erase后继续使用.
			printf("thread tid=========%d exiting\n", it->_tid);//本线程的下一个线程，这里打印是不安全的。因为：当上面erase后，vec元素往前移，
																//若当前非末尾元素被删除不会有问题，因为本语句的it会指向下一个元素，能取到tid。但是存在一种情况，当本语句的it是指向末尾元素，
																//那么vec数组往前移后，此时it指向下一元素为end，所以在it->_tid或者(*it)._tid都会报错，因为不存在该元素。所以下面总结有：
			break;
		}
		else {
			it++;
		}
	}
}

总结问题3）：
	调用erase后，要想在for内继续使用迭代器，必须更新迭代器，否则下一次it++时(假设不满足if条件)，编译器认为你是非法访问。
	并且，即使更新迭代器it后，erase后面也绝对不能再使用更新后的it，因为更新后的it是指向下一元素，而下一元素有可能是end。
	参考vector的具体使用文章：https://blog.csdn.net/weixin_44517656/article/details/110292591
	
4）问题4）
看测试代码：
#include <iostream>
#include <vector>
using namespace std;

struct Item {
public:
	Item(int i) {
		m_i = i;
		cout << "构造函数,i = " << i << endl;
	}
	~Item() {
		cout << "析构函数 i = " << m_i << endl;
	}
	int m_i;
};

class A {
public:
	A() {}

public:

	vector<Item> m_1;
	vector<Item*> m_2;
};

void test01() {

	A a;
	a.m_1.emplace_back(1);
	a.m_1.emplace_back(2);
	a.m_1.emplace_back(3);
	a.m_1.emplace_back(4);
	a.m_1.emplace_back(5);

	for (auto it = a.m_1.begin(); it != a.m_1.end(); ) {
		if (it->m_i == 2) {
			cout << &(*it) << endl;
			a.m_2.emplace_back(&(*it));
			//a.m_2.push_back(&(*it));
			it = a.m_1.erase(it);
			break;
		}
		else {
			it++;
		}
	}

	if (a.m_2.size() >= 0) {
		for (auto it = a.m_2.begin(); it != a.m_2.end();) {
			if ((*it)->m_i == 2) {//不会进来输出ok,因为上面删除后，m1数组元素前移.此时m2数组的m_i=3
				cout << "ok" << endl;
			}
			else {
				it++;
			}
		}
	}

}


int main(int argc, char const *argv[]){

	test01();

	return 0;
}

以上的问题是，当Item通过拷贝进m1数组后，m1删除一个元素前，我们记录该元素的地址到m2，但是当我们删除m1的元素后，由于m1的元素会往前移，导致m2记录的地址的内容发生变化，
最终使m2记录错误的元素地址，可能会发生上面end的非法访问地址错误，所以绝对不能对经过vertor自己拷贝的内存进行地址操作(emplace_back或者push时vector会进行拷贝处理),只能对自己传入的
地址进行处理，例如这里的*_thread线程指针。
即vector自己分配的内容被删除后，不要再使用vector自己分配的内容。

5)new出来的thread不能手动delete，并且使用智能指针new时，在自动回收也会报错。它是会自动回收？这个问题暂时未解决。
