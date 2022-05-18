

//初始化线程池参数，线程池中的线程数量
//向线程池中添加任务，以及线程回调函数，以及工作函数
//I/O密集型可以添加2*n+1个线程，CPU密集型添加n+1个线程
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待请求处理的请求数量*/
    threadpool(int actor_model,connection_pool *connPoool, int thread_number = 4, int max_requests = 10000);
    ~threadpool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);//线程回调函数，静态的
    void run();//工作函数

private:
    int m_thread_number; //线程池中的线程数
    int m_max_requests; //请求队列中允许的最大请求数
    pthread_t *m_threads;//
    std::list<T *> m_workqueue;//请求队列
    locker m_queuelocker; //保护请求队列的互斥锁
    sem m_queuestat;//是否有任务需要处理
    connection_pool *m_connPool; //连接池，单例模式
    int m_actor_mode; //模型切换
};

//创建线程，并设置线程分离
template <typename T>
threadpool<T>::threadpool( int actor_model , connection_pool *connPool,int thread_number, int max_requests) : m_actor_mode(actor_model),m_thread_number(thread_number), m_max_requests(max_requests), m_threads(NULL),m_connPool(connPool)
{
    if (thread_number <= 0|| max_requests <=0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads)
        throw std::exception();
    for (int i = 0;i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL,worker,this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <class T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
}
//添加任务
template <class T>
bool threadpool<T>::append(T *request, int state)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state = state;//设置状态
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//唤醒工作线程
    return true;
}
template <class T>
bool threadpool<T>::append_p(T* request)
{

    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();//唤醒工作线程
    return true;
}

template <class T>
void *threadpool<T>::worker(void *arg)
{

    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <class T>
void threadpool<T>::run()
{
    while( true )
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {

            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;
        if( 1 == m_actor_mode)
        {
            if(0 == request->m_state)
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRALL mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                    if(request->write())
                    {
                        request->improv = 1;

                    }
                    else
                    {
                        request->improv = 1;
                        request->timer_flag = 1;
                    }
                }
            }
            else
            {

                connectionRALL mysqlcon(&request->mysql, m_connPool);
                request->process();
            }

       
    }
}
#endif
