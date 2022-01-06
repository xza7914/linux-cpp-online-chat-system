#ifndef __THREADPOLL_H
#define __THREADPOLL_H

#include <list>
#include <stdio.h>
#include <exception>
#include <pthread.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <fcntl.h>
#include "locker.h"
#include "user.h"

template <typename T>
class Threadpool
{

public:
    Threadpool(int thread_num = 5, int max_requests = 1000);
    ~Threadpool();
    //添加工作
    bool addwork(T *request);
    //线程执行函数
    static void *worker(void *arg);
    //运行线程池
    void run();

private:
    //池中线程数量
    int m_thread_number;
    //最大请求数量
    int m_max_requests;
    //线程号数组
    pthread_t *m_threads;
    //工作队列
    std::list<T *> m_workqueue;
    //工作队列的互斥锁
    locker m_queuelocker;
    //工作时的信号量
    sem m_queuestat;
    //线程的运行状态
    bool m_stop;
};

template <typename T>
Threadpool<T>::Threadpool(int thread_num, int max_requests) : m_thread_number(thread_num), m_max_requests(max_requests), m_threads(NULL), m_stop(false)
{
    if (thread_num <= 0 || max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }

    for (int i = 0; i < m_thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
Threadpool<T>::~Threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool Threadpool<T>::addwork(T *request)
{
    //向工作队列中添加工作，如果工作过多则丢弃
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void Threadpool<T>::run()
{
    //线程不断地从工作队列中取工作并执行s
    while (!m_stop)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (request == NULL)
        {
            continue;
        }
        request->process();

        delete request;
    }
}

template <typename T>
void *Threadpool<T>::worker(void *arg)
{
    Threadpool *pool = (Threadpool *)arg;
    pool->run();
    return pool;
}

#endif