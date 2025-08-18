#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/SqlConnectPool.h"
using namespace std;

template<class T>
class threadpool
{
private:
    int m_thread_number;
    int m_max_requests;
    pthread_t *m_threads;
    list<T*> m_workqueue;
    locker m_queuelocker;
    sem m_queuestat;
    connection_pool *m_connpool;
    int m_actor_model;

    static void *worker(void *args);
    void run();
public:
    threadpool(int actor_model,connection_pool *connpool,int thread_number=8,int max_requests=10000);
    ~threadpool();
    bool append_r(T *request,int state);
    bool append_p(T *request);
};

template<class T>
threadpool<T>::threadpool(int actor_model,connection_pool *connpool,int thread_number=8,int max_requests=10000):
m_actor_model(actor_model),connpool(connpool),m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL)
{
    if(thread_number<0||max_requests<0)
    {
        throw exception();
    }
    m_threads=new pthread_t[thread_number];
    if(!m_threads)
    {
        throw exception();
    }
    for(int i=0;i<thread_number;i++)
    {
        if(pthread_create(m_threads+i,NULL,worker,this)!=0)
        {
            delete []m_threads;
            throw exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete []m_threads;
            throw exception();
        }
    }
}

template<class T>
threadpool<T>::~threadpool()
{
    delete []m_threads;
}


template<class T>
bool threadpool<T>::append_r(T *request,int state)
{
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state=state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<class T>
bool threadpool<T>::append_p(T *request)
{
    m_queuelocker.lock();
    if(m_workqueue.size()>m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<class T>
void *threadpool<T>::worker(void *args)
{
    threadpool *pool=(threadpool *)args;
    pool->run();
    return pool;
}

template<class T>
void threadpool<T>::run()
{
    while(true)
    {
    m_queuestat.wait();
    m_queuelocker.lock();
    if(m_workqueue.empty())
    {
        m_queuelocker.unlock();
        continue;
    }
    T *request=m_workqueue.front();
    m_workqueue.pop_front();
    m_queuelocker.unlock();
    if(!request) continue;
    // Reactor模式处理
        if (1 == m_actor_model)
        {
            // 读事件处理
            if (0 == request->m_state)
            {
                // 尝试读取数据
                if (request->read_once())
                {
                    request->improv = 1;  // 标记处理完成
                    // 从连接池获取数据库连接（RAII方式自动管理）
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();  // 执行业务逻辑
                }
                else
                {
                    // 读取失败处理
                    request->improv = 1;
                    request->timer_flag = 1;  // 标记定时器事件
                }
            }
            // 写事件处理
            else
            {
                // 尝试写入数据
                if (request->write())
                {
                    request->improv = 1;  // 标记处理完成
                }
                else
                {
                    // 写入失败处理
                    request->improv = 1;
                    request->timer_flag = 1;  // 标记定时器事件
                }
            }
        }
        // Proactor模式处理
        else
        {
            // 获取数据库连接
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            // 直接处理完整请求（I/O操作已完成）
            request->process();  // 执行业务逻辑
        }
    }
}
#endif