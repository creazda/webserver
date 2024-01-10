#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__
#include <memory>
#include <vector>
#include "sylar/mutex.h"
#include "sylar/thread.h"
#include "sylar/fiber.h"
#include <list>
#include <iostream>
#include <atomic>
#include "log.h"
namespace sylar
{

    class Scheduler
    {
    public:
        typedef std::shared_ptr<Scheduler> ptr;
        typedef Mutex MutexType;

        Scheduler(size_t threads = 1, bool use_caller = true, const std::string &name = "");
        virtual ~Scheduler();

        const std::string &GetName() const { return mName; }

        static Scheduler *GetThis();
        static sylar::Fiber *GetMainFiber();

        void Stop();
        void Start();

        template <class FiberOrCb>
        void schedule(FiberOrCb fc, int threadid = -1)
        {
            bool need_tickle = false;
            {
                MutexType::MutexLock lock(mMutex);
                need_tickle = scheduleNoLock(fc, threadid);
            }
            if (need_tickle)
            {
                Tickle();
            }
        }

        template <class InputIterator>
        void schedule(InputIterator begin, InputIterator end)
        {
            bool need_tickle = false;
            {
                MutexType::MutexLock lock(mMutex);
                while (begin != end)
                {
                    need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                    ++begin;
                }
            }
            if (need_tickle)
            {
                Tickle();
            }
        }

    protected:
        virtual void Tickle();
        void Run(); // 真正执行协程调度的方法
        virtual bool Stopping();
        virtual void Idle();
        void SetThis();

        bool HasIdleThreads() { return mIdleThreadCount > 0; }

    private:
        template <class FiberOrCb>
        bool scheduleNoLock(FiberOrCb fc, int threadid)
        {
            std::cout << "注册任务" << std::endl;
            bool need_tickle = mFibers.empty();
            FiberAndThread ft(fc, threadid);
            if (ft.fiber || ft.cb)
            {
                mFibers.push_back(ft);
            }
            return need_tickle;
        }

    private:
        struct FiberAndThread
        {
            sylar::Fiber::ptr fiber;
            std::function<void()> cb;
            int threadid;

            FiberAndThread(sylar::Fiber::ptr f, int thrid)
                : fiber(f), threadid(thrid)
            {
            }
            FiberAndThread(sylar::Fiber::ptr *f, int thrid)
                : threadid(thrid)
            {
                fiber.swap(*f);
            }
            FiberAndThread(std::function<void()> f, int thrid)
                : cb(f), threadid(thrid)
            {
            }
            FiberAndThread(std::function<void()> *f, int thrid)
                : threadid(thrid)
            {
                cb.swap(*f);
            }
            FiberAndThread()
                : threadid(-1)
            {
            }
            void Reset()
            {
                fiber = nullptr;
                cb = nullptr;
                threadid = -1;
            }
            /* data */
        };

    private:
        MutexType mMutex;                  // Mutex
        std::vector<Thread::ptr> mThreads; // 线程池
        std::list<FiberAndThread> mFibers; // 即将执行或要执行的一些协程
        sylar::Fiber::ptr mRootFiber;      // 主协程(调度协程,use_caller为true时有效)
        std::string mName;

    protected:
        // 协程下的线程id数组
        std::vector<int> mThreadIds;
        // 线程数量
        size_t mThreadCount = 0;
        // 工作线程数量
        std::atomic<size_t> mActiveThreadCount = {0};
        // 空闲线程数量
        std::atomic<size_t> mIdleThreadCount = {0};
        // 是否正在停止
        bool mStopping = true;
        // 是否自动停止
        bool mAutoStop = false;
        // 主线程ID
        int mRootThreadId = 0;
    };
}
#endif