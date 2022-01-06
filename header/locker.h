#ifndef __LOCKER_H
#define __LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>
#include <unistd.h>

class locker
{
public:
    locker()
    {
        if (pthread_mutex_init(&m_lock, NULL) != 0)
        {
            throw std::exception();
        }
    }

    ~locker()
    {
        pthread_mutex_destroy(&m_lock);
    }

    int lock()
    {
        return pthread_mutex_lock(&m_lock);
    }

    int unlock()
    {
        return pthread_mutex_unlock(&m_lock);
    }

    int trylock()
    {
        return pthread_mutex_trylock(&m_lock);
    }

private:
    pthread_mutex_t m_lock;
};

class sem
{
public:
    sem(int value = 0)
    {
        if (sem_init(&m_sem, 0, value) != 0)
        {
            throw std::exception();
        }
    }

    ~sem()
    {
        sem_destroy(&m_sem);
    }

    int wait()
    {
        return sem_wait(&m_sem);
    }

    int post()
    {
        return sem_post(&m_sem);
    }

private:
    sem_t m_sem;
};

class spinlock
{
public:
    spinlock()
    {
        if (pthread_spin_init(&s_lock, PTHREAD_PROCESS_PRIVATE) != 0)
        {
            throw std::exception();
        }
    }

    ~spinlock()
    {
        pthread_spin_destroy(&s_lock);
    }

    int lock()
    {
        return pthread_spin_lock(&s_lock);
    }

    int unlock()
    {
        return pthread_spin_unlock(&s_lock);
    }

private:
    pthread_spinlock_t s_lock;
};

#endif