#include "thread.h"
#include "scheduler.h"
#include <stdlib.h>
#include "fiber.h"
#include <atomic>
#include "macro.h"
#include "config.h"
#include "log.h"

namespace sylar
{
    static Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static std::atomic<uint64_t> s_fiber_id(0);
    static std::atomic<uint64_t> s_fiber_count(0);

    static thread_local Fiber *t_fiber = nullptr;           // 当前协程
    static thread_local Fiber::ptr t_threadFiber = nullptr; // 主协程

    static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
        Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

    class MallocStackAllocator
    {
    public:
        static void *Alloc(size_t size)
        {
            return malloc(size);
        }
        static void Dealloc(void *vp, size_t size)
        {
            return free(vp);
        }
    };

    using StackAllocator = MallocStackAllocator;

    uint64_t Fiber::GetFiberId()
    {
        if (t_fiber)
        {
            return t_fiber->GetId();
        }
        return 0;
    }

    Fiber::Fiber()
    {
        mState = EXEC;
        SetThis(this);

        if (getcontext(&mCtx))
        {
            SYLAR_ASSERT2(false, "getcontext");
        }
        ++s_fiber_count;
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::Fiber()";
    }

    Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool use_caller)
        : mId(++s_fiber_id), mCb(cb)
    {
        ++s_fiber_count;
        mStacksize = stacksize ? stacksize : g_fiber_stack_size->GetValue();

        mStack = StackAllocator::Alloc(mStacksize);
        if (getcontext(&mCtx))
        {
            SYLAR_ASSERT2(false, "getcontext");
        }
        mCtx.uc_link = nullptr;
        mCtx.uc_stack.ss_sp = mStack;       // 栈的起始位置
        mCtx.uc_stack.ss_size = mStacksize; // 栈大小

        if (!use_caller)
        {
            makecontext(&mCtx, &Fiber::MainFunc, 0);
        }
        else
        {
            makecontext(&mCtx, &Fiber::CallerMainFunc, 0);
        }
        SYLAR_LOG_DEBUG(g_logger)
            << "Fiber::Fiber()) ID=" << mId;
    }
    Fiber::~Fiber()
    {
        --s_fiber_count;

        if (mStack)
        { // 说明不是主协程，能被析构的要不就是结束了，要不就是还在初始化
            SYLAR_ASSERT(mState == TERM || mState == INIT || mState == EXCEPT);
            StackAllocator::Dealloc(mStack, mStacksize);
        }
        else
        {                                 // 主协程
            SYLAR_ASSERT(!mCb);           // 无cb
            SYLAR_ASSERT(mState == EXEC); // 执行中

            Fiber *cur = t_fiber;
            if (cur == this)
            {
                SetThis(nullptr);
            }
        }
        SYLAR_LOG_DEBUG(g_logger) << "Fiber::~Fiber(std::function<void()> cb, size_t stacksize) ID=" << mId;
    }
    // 重置协程函数，并且重置状态,当一个协程执行完毕之后，可以重新创建一个协程来利用空间
    void Fiber::Reset(std::function<void()> cb)
    {
        SYLAR_ASSERT(mStack);
        SYLAR_ASSERT(mState == TERM || mState == INIT || mState == EXCEPT); // 只有在这个状态才可以reset
        mCb = cb;
        if (getcontext(&mCtx)) // 返回0为成功
        {
            SYLAR_ASSERT2(false, "getcontext");
        }
        mCtx.uc_link = nullptr;
        mCtx.uc_stack.ss_sp = mStack;
        mCtx.uc_stack.ss_size = mStacksize;

        makecontext(&mCtx, &Fiber::MainFunc, 0);
        mState = INIT;
    }
    // 切换到当前协程执行
    void Fiber::SwapIn()
    {
        SetThis(this);
        SYLAR_ASSERT(mState != EXEC);
        mState = EXEC;
        if (swapcontext(&Scheduler::GetMainFiber()->mCtx, &mCtx))
        { // 保存第一个参数的上下文并且切换到第二个参数的上下文
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
    // 切换到后台执行
    void Fiber::SwapOut()
    {
        SetThis(Scheduler::GetMainFiber());
        if (swapcontext(&mCtx, &Scheduler::GetMainFiber()->mCtx))
        {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
    void Fiber::Call()
    {
        SetThis(this);
        mState = EXEC;
        if (swapcontext(&t_threadFiber->mCtx, &mCtx))
        {
            SYLAR_ASSERT2(false, "Call");
        }
    }
    void Fiber::Back()
    {
        SetThis(t_threadFiber.get());
        if (swapcontext(&mCtx, &t_threadFiber->mCtx))
        {
            SYLAR_ASSERT2(false, "swapcontext");
        }
    }
    // 设置当前协程
    void Fiber::SetThis(Fiber *f)
    {
        t_fiber = f;
    }
    // 返回当前协程,如果没有则创建主协程并返回
    Fiber::ptr Fiber::GetThis()
    {
        if (t_fiber)
        {
            return t_fiber->shared_from_this();
        }
        Fiber::ptr mainFiber(new Fiber);
        SYLAR_ASSERT(t_fiber == mainFiber.get());
        t_threadFiber = mainFiber;
        return t_fiber->shared_from_this();
    }
    // 协程切换到后台，并且设置为Ready
    void Fiber::YieldToReady()
    {
        Fiber::ptr cur = GetThis();
        cur->mState = READY;
        cur->SwapOut();
    }
    // 协程切换到后台，并且设置为Hold
    void Fiber::YieldToHold()
    {
        Fiber::ptr cur = GetThis();
        cur->mState = HOLD;
        cur->SwapOut();
    }
    // 总协程数
    uint64_t Fiber::ToTalFibers()
    {
        return s_fiber_count;
    }
    void Fiber::MainFunc()
    {
        // SYLAR_LOG_INFO(g_logger) << "----------------";
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try
        {
            cur->mCb();
            cur->mCb = nullptr;
            cur->mState = TERM;
        }
        catch (const std::exception &e)
        {
            cur->mState = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
                                      << " fiber_id=" << cur->GetId()
                                      << std::endl
                                      << sylar::BacktraceToString();
        }
        catch (...)
        {
            cur->mState = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except "
                                      << " fiber_id=" << cur->GetId()
                                      << std::endl
                                      << sylar::BacktraceToString();
        }

        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->SwapOut();
        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->GetId()));
    }
    void Fiber::CallerMainFunc()
    {
        // SYLAR_LOG_INFO(g_logger) << "++++++++++++++";

        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
        try
        {
            cur->mCb();
            cur->mCb = nullptr;
            cur->mState = TERM;
        }
        catch (const std::exception &e)
        {
            cur->mState = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: " << e.what()
                                      << " fiber_id=" << cur->GetId()
                                      << std::endl
                                      << sylar::BacktraceToString();
        }
        catch (...)
        {
            cur->mState = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except "
                                      << " fiber_id=" << cur->GetId()
                                      << std::endl
                                      << sylar::BacktraceToString();
        }

        auto raw_ptr = cur.get();
        cur.reset();
        raw_ptr->Back();
        SYLAR_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->GetId()));
    }
}