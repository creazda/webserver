#include "sylar/mutex.h"
namespace sylar
{

    Semaphore::Semaphore(uint32_t count)
    {
        // sem：指向要初始化的信号量的指针。
        // pshared：指定信号量的类型。如果pshared的值为0，则信号量是在进程内共享的（线程之间共享）。如果pshared的值不为0，则信号量是在进程之间共享的（不同进程的线程之间共享）。
        // value：指定信号量的初始值。
        if (sem_init(&mSemaphore, 0, count))
        {
            throw std::logic_error("sem_init error");
        }
    }
    Semaphore::~Semaphore()
    {
        sem_destroy(&mSemaphore);
    }

    void Semaphore::wait()
    {
        if (sem_wait(&mSemaphore))
        {
            throw std::logic_error("sem_wait error");
        }
    }
    void Semaphore::notify()
    {
        if (sem_post(&mSemaphore))
        {
            throw std::logic_error("sem_post error");
        }
    }
}
