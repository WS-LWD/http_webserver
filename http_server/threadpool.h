#pragma once
#include<iostream>
#include <cstdio>
#include <exception>
#include<pthread.h>
#include<list>
#include"lock_signal_sem.h"
using namespace std;
 
// 线程池类，将其定义为模板类是为了代码复用，模板参数T是任务类
template<typename T>
class threadpool
{
private:
    pthread_t *m_threads;               //线程组
    int m_thread_number = 10;           //线程数量
    int m_thread_busy = 0;
    int m_thread_idle = 0;
    std::list<T*> m_workqueue;          //请求队列
    int m_queue_number = 10000;         //请求队列中等待处理的请求的最大容量
    locker m_queuelock;                 //保护请求队列的互斥锁
    cond m_threadcond;                  //唤醒线程处理请求队列中的请求
    bool m_stop;                        //是否结束线程
private:
    static void *work(void *arg);       //回调函数
    void run();                         //等待唤醒执行请求
public:
    threadpool();                       //创建线程池
    ~threadpool();
    bool append(T *requests);                      //加入请求队列
};


template<typename T>
threadpool<T>::threadpool() : m_threads(nullptr), m_stop(false)
{
    if(m_thread_number <= 0 || m_queue_number <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
    {
        throw std::exception();
    }
    cout<< "线程池可用线程：" << endl;
    for(int i = 0; i < m_thread_number; i++)
    {
        m_thread_idle++;
        printf("第%d个线程\n", i);
        if(pthread_create(&m_threads[i], NULL, work, this) != 0)
        {
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]) != 0)
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;
    m_stop=true;
}

template<typename T>
bool threadpool<T>::append(T *requests)
{
    m_queuelock.lock();
    if(m_workqueue.size() >= m_queue_number)
    {
        m_queuelock.unlock();
    }
    m_workqueue.push_back(requests);
    m_threadcond.signal();
    m_queuelock.unlock();
    return true;
}

template<typename T>
void* threadpool<T>::work(void *arg)
{
    threadpool *pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        m_queuelock.lock();
        auto lock = m_queuelock.get();
        while(m_workqueue.empty())
        {
            m_threadcond.wait(lock);
        }
        m_thread_busy++;
        m_thread_idle--;
        cout << "m_thread_busy = " << m_thread_busy << '\t' << "m_thread_idle = " << m_thread_idle << endl;
        auto requests = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelock.unlock();
        if(!requests){
            continue;
        }
        cout << "tid = " << pthread_self() << endl;
        requests->process(); // 最终调用模板参数的T::process()处理
        m_thread_busy--;
        m_thread_idle++;
        cout << "m_thread_busy = " << m_thread_busy << '\t' << "m_thread_idle = " << m_thread_idle << endl;
        cout << "解放tid-------------------------------------" <<  pthread_self() << endl;
    }
}