#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__

#include <memory>
#include <functional>
#include <ucontext.h>

namespace sylar
{
    class Scheduler;
    class Fiber : public std::enable_shared_from_this<Fiber>
    {
        friend class Scheduler;

    public:
        typedef std::shared_ptr<Fiber> ptr;
        enum State
        {
            INIT,
            HOLD,
            EXEC,
            TERM,
            READY,
            EXCEPT
        };

    private:
        // 主协程
        Fiber();

    public:
        Fiber(std::function<void()> cb, size_t stacksize = 0, bool use_caller = false);
        ~Fiber();
        // 重置协程函数，并且重置状态
        void Reset(std::function<void()> cb);
        // 切换到当前协程执行
        void SwapIn();
        // 切换到后台执行
        void SwapOut();
        void Call();
        void Back();
        uint64_t GetId() const { return mId; }

        State GetState() const { return mState; }

    public:
        // 设置当前协程
        static void SetThis(Fiber *f);
        // 返回当前协程,如果当前线程没有协程，创建一个主协程
        static Fiber::ptr GetThis();
        // 协程切换到后台，并且设置为Ready
        static void YieldToReady();
        // 协程切换到后台，并且设置为Ready
        static void YieldToHold();
        // 总协程数
        static uint64_t ToTalFibers();
        static void MainFunc();
        static void CallerMainFunc();
        static uint64_t GetFiberId();

    private:
        uint64_t mId = 0;
        uint32_t mStacksize = 0;
        State mState = INIT;

        ucontext_t mCtx;
        void *mStack = nullptr;

        std::function<void()> mCb;
    };

} // namespace sylar

#endif
