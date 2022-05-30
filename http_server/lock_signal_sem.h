#pragma once

#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 线程同步封装类

// 互斥锁类
class locker{
public:
    locker(){   //初始化互斥锁
        if(pthread_mutex_init(&m_mutex, nullptr) != 0){  
            throw std::exception();
        }
    }
    ~locker(){
        pthread_mutex_destroy(&m_mutex);        //销毁互斥锁
    }
    bool lock(){
        return pthread_mutex_lock(&m_mutex) == 0;   //加锁
    }
    bool unlock(){
        return pthread_mutex_unlock(&m_mutex) == 0; //解锁
    }
    pthread_mutex_t* get(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

// 条件变量
class cond{
public:
    cond(){     //初始化条件变量
        if(pthread_cond_init(&m_cond, nullptr) != 0){
            throw std::exception();
        }
    }
    ~cond(){    //销毁条件变量
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t* m_mutex){    //等待唤醒
        int ret=0;
        ret=pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t* m_mutex, struct timespec t){
        int ret=0;
        ret=pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal(){
        return pthread_cond_signal(&m_cond) == 0;   //唤醒正在等待的至少  1个  线程
    }
    bool broadcast(){
        return pthread_cond_broadcast(&m_cond) == 0; //唤醒正在等待的   多个  线程
    }

private:
    pthread_cond_t m_cond;

};

// 信号量类
class sem{
public:
    sem(){
        if(sem_init(&m_sem, 0, 0) != 0){
            throw std::exception();
        }
    }
    sem(int num){   
        if(sem_init(&m_sem, 0, num) != 0){
            throw std::exception();
        }
    }
    ~sem(){
        sem_destroy(&m_sem);
    }
    // 等待信号量
    bool wait(){
        return sem_wait(&m_sem) == 0;
    }
    // 增加信号量
    bool post(){
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};
