#include "sylar/scheduler.h"
#include "log.h"
#include "macro.h"
#include "config.h"
#include "hook.h"

namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    static thread_local Scheduler *t_scheduler = nullptr; // 每个线程中的调度器指针。
    static thread_local Fiber *t_fiber = nullptr;         // 每个线程中的执行调度函数的协程

    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
        : mName(name)
    {
        SYLAR_ASSERT(threads > 0);

        if (use_caller)
        {
            sylar::Fiber::GetThis(); // 如果没有主协程，直接创建一个
            --threads;

            SYLAR_ASSERT(GetThis() == nullptr); // 断言现在没有协程调度器
            t_scheduler = this;

            mRootFiber.reset(new Fiber(std::bind(&Scheduler::Run, this), 0, true));
            sylar::Thread::S_SetName(mName);

            t_fiber = mRootFiber.get();
            mRootThreadId = sylar::UtilGetThreadId();
            mThreadIds.push_back(mRootThreadId);
        }
        else
        {
            mRootThreadId = -1;
        }
        mThreadCount = threads;
    }
    Scheduler::~Scheduler()
    {
        SYLAR_ASSERT(mStopping);
        if (GetThis() == this)
        {
            t_scheduler = nullptr;
        }
    }

    Scheduler *Scheduler::GetThis()
    {
        return t_scheduler;
    }
    Fiber *Scheduler::GetMainFiber()
    {
        return t_fiber;
    }

    // 分两种情况，一种是use_caller，那么就要在创建scheduler的线程中执行stop，否则就可以在任何非自己的线程中执行stop
    void Scheduler::Stop()
    {
        SYLAR_LOG_INFO(g_logger) << "stop";

        mAutoStop = true;
        // rootfiber 创建scheduler的线程的那个run的协程。这个判断就是说，当创建scheduler的线程的协程状态为INIT或者TERM
        if (mRootFiber && mThreadCount == 0 && (mRootFiber->GetState() == Fiber::INIT || mRootFiber->GetState() == Fiber::TERM))
        {
            SYLAR_LOG_INFO(g_logger) << this << " stopped";
            mStopping = true;

            if (Stopping())
            {
                return;
            }
        }

        // bool exit_on_this_fiber = false;
        // {std::cout << "mRootThreadId" << mRootThreadId << std::endl;
        // std::cout << "GetThis()" << GetThis() << std::endl;
        // std::cout << "this()" << this << std::endl;}

        if (mRootThreadId != -1)
        { // use_caller
            SYLAR_ASSERT(GetThis() == this);
        }
        else
        {
            SYLAR_ASSERT(GetThis() != this);
        }

        mStopping = true;
        for (size_t i = 0; i < mThreadCount; i++)
        {
            Tickle(); // 唤醒线程，自己把自己结束。类似于信号量
        }
        if (mRootFiber)
        {
            Tickle();
        }

        if (mRootFiber)
        {
            if (!Stopping())
            {
                SYLAR_LOG_INFO(g_logger) << "mRootFiber 执行";
                mRootFiber->Call();
            }
        }

        // 主线程等待其他线程执行结束，然后才可以完成stop
        std::vector<Thread::ptr> thrs;
        {
            MutexType::MutexLock lock(mMutex);
            thrs.swap(mThreads);
        }
        for (auto &i : thrs)
        {
            i->Join();
        }
    }
    void Scheduler::Start()
    {
        SYLAR_LOG_INFO(g_logger) << "start";
        MutexType::MutexLock lock(mMutex);
        if (!mStopping)
        { // 已经在运行就返回
            return;
        }
        mStopping = false;
        SYLAR_ASSERT(mThreads.empty());

        mThreads.resize(mThreadCount);
        for (size_t i = 0; i < mThreadCount; i++)
        {
            SYLAR_LOG_INFO(g_logger) << "创建线程";
            mThreads[i].reset(new Thread(std::bind(&Scheduler::Run, this), mName + "_" + std::to_string(i)));
            mThreadIds.push_back(mThreads[i]->GetId());
            // std::cout << "this: " << this << std::endl;
        }
        lock.UnLock();
        // if (mRootFiber)
        // {
        //     // mRootFiber->SwapIn();
        //     mRootFiber->Call();
        //     SYLAR_LOG_INFO(g_logger) << "call  out ";
        // }
    }
    void Scheduler::SetThis()
    {
        t_scheduler = this;
        // std::cout << "this: " << &(*this) << std::endl;
        // std::cout << "t_schedule: " << &(*t_scheduler) << std::endl;
    }
    void Scheduler::Run()
    {
        SYLAR_LOG_INFO(g_logger) << "run begin";
        set_hook_enable(true);
        // std::cout << "t_schedule: " << &(*t_scheduler) << std::endl;
        SetThis();
        // std::cout << "this: " << &(*this) << std::endl;

        if (sylar::UtilGetThreadId() != mRootThreadId)
        { // 如果执行线程id不等于初始化协程调度器的线程的id，那么执行协程调度函数的协程要改为本线程的某一线程。
            t_fiber = Fiber::GetThis().get();
        }
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::Idle, this))); // 空转休闲函数
        Fiber::ptr cb_fiber;                                                 // 传入的如果是func而不是fiber的时候需要用

        FiberAndThread ft;
        int i = 1;
        while (true)
        {
            {
                SYLAR_LOG_INFO(g_logger) << "第" << i << "轮执行";
                i++;
            }
            ft.Reset();
            bool tickle_me = false; // 是否需要激活下一个线程来领任务
            bool isActive = false;  // 是否有任务在做
            {
                MutexType::MutexLock lock(mMutex);
                auto it = mFibers.begin();
                while (it != mFibers.end())
                {
                    if (it->threadid != -1 && it->threadid != sylar::UtilGetThreadId())
                    { // 表明这个任务有指定的线程,且不是这个线程
                        ++it;
                        tickle_me = true; // 需要唤醒其他线程执行
                        continue;
                    }
                    SYLAR_ASSERT(it->fiber || it->cb); // 断言这个任务有fiber或者func
                    if (it->fiber && it->fiber->GetState() == Fiber::EXEC)
                    { // 如果有fiber且正在执行
                        it++;
                        continue;
                    }
                    ft = *it;
                    mFibers.erase(it);
                    ++mActiveThreadCount;
                    isActive = true;
                    break;
                }
            }
            if (tickle_me)
            {
                Tickle();
            }
            if (ft.fiber && ft.fiber->GetState() != Fiber::TERM && ft.fiber->GetState() != Fiber::EXCEPT)
            { // 如果是fiber且状态没结束
                //++mActiveThreadCount; 写在这里的话有可能出现线程安全问题。
                SYLAR_LOG_INFO(g_logger) << "----fiber 执行";
                ft.fiber->SwapIn();
                --mActiveThreadCount;
                if (ft.fiber->GetState() == Fiber::READY)
                { // 执行完毕后仍然为reday，需要重新放入执行队列
                    schedule(ft.fiber);
                }
                else if (ft.fiber->GetState() != Fiber::TERM && ft.fiber->GetState() != Fiber::EXCEPT)
                { // 如果不是正常结束或者异常退出，把ft.fiebr 的状态置为HOLD
                    ft.fiber->mState = Fiber::HOLD;
                }
                ft.Reset();
            }
            else if (ft.cb)
            { // func
                if (cb_fiber)
                {
                    cb_fiber->Reset(ft.cb);
                }
                else
                {
                    cb_fiber.reset(new Fiber(ft.cb));
                }
                ft.Reset();
                // ++mActiveThreadCount; // 统一写在领取任务的时候了
                SYLAR_LOG_INFO(g_logger) << "----cb 执行";

                cb_fiber->SwapIn();
                --mActiveThreadCount;
                if (cb_fiber->GetState() == Fiber::READY)
                { // 执行完毕后仍然为reday，需要重新放入执行队列,这次是以fiber形式放入
                    schedule(cb_fiber);
                    cb_fiber.reset();
                }
                else if (cb_fiber->GetState() == Fiber::EXCEPT || cb_fiber->GetState() == Fiber::TERM)
                { // 异常退出或者正常结束，临时定义的reset一下
                    cb_fiber->Reset(nullptr);
                }
                else
                { // 其他情况 ，设置为HOLD。并且cb_fiber重新指向
                    cb_fiber->mState = Fiber::HOLD;
                    cb_fiber.reset();
                }
            }
            else
            {
                if (isActive)
                { // 当拿到的任务不属于上面的两个的时候，虽然isactive被置为true，但是，也需要--mActiveThreadCount，防止被恶意使用bug

                    // std::cout << "is active" << std::endl;
                    --mActiveThreadCount;
                    continue;
                }
                // SYLAR_LOG_INFO(g_logger) << "无任务，准备swapin idle";
                if (idle_fiber->GetState() == Fiber::TERM)
                { // idle_fiber执行完毕

                    SYLAR_LOG_INFO(g_logger) << "idle fiber term";
                    break;
                }

                ++mIdleThreadCount;
                SYLAR_LOG_INFO(g_logger) << "----idle 执行";
                idle_fiber->SwapIn();
                if (idle_fiber->GetState() != Fiber::TERM && idle_fiber->GetState() != Fiber::EXCEPT)
                { // 既不成功又不异常置为hold
                    idle_fiber->mState = Fiber::HOLD;
                }
                --mIdleThreadCount;
            }
        }
    }

    void Scheduler::Tickle()
    {
        SYLAR_LOG_INFO(g_logger) << "tickle";
    }

    bool Scheduler::Stopping()
    {
        MutexType::MutexLock lock(mMutex);
        return mAutoStop && mStopping && mFibers.empty() && mActiveThreadCount == 0;
    }
    void Scheduler::Idle()
    {
        SYLAR_LOG_INFO(g_logger) << "idle";
        while (!Stopping())
        {
            sylar::Fiber::YieldToHold();
        }
    }
}