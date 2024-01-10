#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__
#include <thread>
#include <semaphore.h>
#include <pthread.h>
#include <memory>
#include <stdint.h>
#include <list>
#include <functional>
#include <iostream>
#include <atomic>
#include "noncopyable.h"
namespace sylar
{
    // Semaphore
    class Semaphore : Noncopyable
    {
    public:
        Semaphore(uint32_t count = 0);
        ~Semaphore();

        void wait();
        void notify();

    private:
        sem_t mSemaphore;
    };

    // ScopedLockImpl
    template <class T>
    struct ScopedLockImpl
    {
    public:
        ScopedLockImpl(T &mutex)
            : mMutex(mutex)
        {
            mMutex.Lock();
            mLocked = true;
        }
        ~ScopedLockImpl()
        {
            UnLock();
        }

        void Lock()
        {
            if (!mLocked)
            {
                mMutex.Lock();
                mLocked = true;
            }
        }
        void UnLock()
        {
            if (mLocked)
            {
                mMutex.UnLock();
                mLocked = false;
            }
        }

    private:
        T &mMutex;
        bool mLocked;
    };

    // ReadScopedLockImpl
    template <class T>
    struct ReadScopedLockImpl
    {
    public:
        ReadScopedLockImpl(T &mutex)
            : mMutex(mutex)
        {
            mMutex.RdLock();
            mLocked = true;
        }
        ~ReadScopedLockImpl()
        {
            UnLock();
        }

        void Lock()
        {
            if (!mLocked)
            {
                mMutex.RdLock();
                mLocked = true;
            }
        }
        void UnLock()
        {
            if (mLocked)
            {
                mMutex.UnLock();
                mLocked = false;
            }
        }

    private:
        T &mMutex;
        bool mLocked;
    };
    // WriteScopedLockImpl
    template <class T>
    struct WriteScopedLockImpl
    {
    public:
        WriteScopedLockImpl(T &mutex)
            : mMutex(mutex)
        {
            mMutex.WrLock();
            mLocked = true;
        }
        ~WriteScopedLockImpl()
        {
            UnLock();
        }

        void Lock()
        {
            if (!mLocked)
            {
                mMutex.WrLock();
                mLocked = true;
            }
        }
        void UnLock()
        {
            if (mLocked)
            {
                mMutex.UnLock();
                mLocked = false;
            }
        }

    private:
        T &mMutex;
        bool mLocked;
    };

    // 互斥锁
    class Mutex : Noncopyable
    {
    public:
        typedef ScopedLockImpl<Mutex> MutexLock;
        Mutex()
        {
            pthread_mutex_init(&mMutex, nullptr);
        }
        ~Mutex()
        {
            pthread_mutex_destroy(&mMutex);
        }
        void Lock()
        {
            pthread_mutex_lock(&mMutex);
        }
        void UnLock()
        {
            pthread_mutex_unlock(&mMutex);
        }

    private:
        pthread_mutex_t mMutex;
    };

    // 空实现锁
    class NullMutex : Noncopyable
    {
    public:
        typedef ScopedLockImpl<NullMutex> MutexLock;
        NullMutex() {}
        ~NullMutex() {}
        void Lock() {}
        void UnLock() {}
    };
    // 读写锁
    class RWMutex : Noncopyable
    {
    public:
        typedef ReadScopedLockImpl<RWMutex> ReadLock;
        typedef WriteScopedLockImpl<RWMutex> WriteLock;

        RWMutex()
        {
            pthread_rwlock_init(&mLock, nullptr);
        }
        ~RWMutex()
        {
            pthread_rwlock_destroy(&mLock);
        }

        void WrLock()
        {
            pthread_rwlock_wrlock(&mLock);
        }
        void RdLock()
        {
            pthread_rwlock_rdlock(&mLock);
        }
        void UnLock()
        {
            pthread_rwlock_unlock(&mLock);
        }

    private:
        pthread_rwlock_t mLock;
    };

    class NullRWMutex : Noncopyable
    {
    public:
        typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
        typedef WriteScopedLockImpl<NullRWMutex> WriteLock;
        NullRWMutex() {}
        ~NullRWMutex() {}
        void RdLock() {}
        void WrLock() {}
        void UnLock() {}
    };
    class Spinlock : Noncopyable
    {
    public:
        typedef ScopedLockImpl<Spinlock> MutexLock;
        Spinlock()
        {
            pthread_spin_init(&mMutex, 0);
        }
        ~Spinlock()
        {
            pthread_spin_destroy(&mMutex);
        }
        void Lock()
        {
            pthread_spin_lock(&mMutex);
        }
        void UnLock()
        {
            pthread_spin_unlock(&mMutex);
        }

    private:
        pthread_spinlock_t mMutex;
    };
    class CASLock : Noncopyable
    {
    public:
        typedef ScopedLockImpl<CASLock> MutexLock;
        CASLock()
        {
            mMutex.clear();
        }
        ~CASLock()
        {
        }
        void Lock()
        {
            // 当前操作之前的读操作不会排在当前操作之后
            while (std::atomic_flag_test_and_set_explicit(&mMutex, std::memory_order_acquire))
                ;
        }
        void UnLock()
        {
            // 当前操作之后的写操作不会排在当前操作之前
            std::atomic_flag_clear_explicit(&mMutex, std::memory_order_release);
        }

    private:
        volatile std::atomic_flag mMutex;
    };

}

#endif // __SYLAR_MUTEX_H__
