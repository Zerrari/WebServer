#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <iostream>
#include "locker.h"

template<typename T>
class threadpool
{
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    // 往请求队列中添加任务
    bool append(T* request);

private:
    // 工作线程运行的函数
    static void* worker(void* arg);
    // 从请求队列中取任务运行
    void run();

private: 
    // 最大可容纳的线程数
    int m_thread_number;
    // 最大可接受的请求数
    int m_max_requests;
    // 线程数组
    pthread_t* m_threads;
    // 工作队列
    std::list<T*> m_workqueue;
    // 保护请求队列的互斥锁
    locker m_queuelocker;
    // 是否有任务需要处理,用信号量来表示
    sem m_queuestat;
    // 是否结束线程
    bool m_stop;
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_stop(false)
{
    // 如果给定线程数或者请求数不符合要求，抛出异常
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }

    // 建立线程数组
    m_threads = new pthread_t[m_thread_number];
    //检查动态内存分配是否成功
    if(!m_threads)
    {
        throw std::exception();
    }

    // 循环调用pthread_create建立线程
    for (int i = 0; i < thread_number; ++i)
    {
        printf("create the %dth thread\n", i);
        // 建立线程失败，抛出错误
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete [] m_threads;
            throw std::exception();
        }
        // 分离父子线程，子线程死亡时，资源由自己来释放
        // 分离父子线程失败，则抛出错误
        if (pthread_detach(m_threads[i]))
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

// 析构函数
template<typename T>
threadpool<T>::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

// 往请求队列中添加请求任务
template<typename T>
bool threadpool<T>::append(T* request)
{
    // 操作请求队列时加锁，队列被所有线程共享
    m_queuelocker.lock();

    // 如果请求队列大小大于最大请求数
    // 释放锁，返回添加失败的结果
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    // 信号量传递
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg)
{
    threadpool* pool = (threadpool*)arg;
    // 启动线程池
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        // 等待信号量
        m_queuestat.wait();
        // 对请求队列加锁
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        request->process();
    }
}

#endif
