#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__

#include <memory>
#include <functional>
#include <set>
#include <vector>
#include "mutex.h"

namespace sylar
{
    class TimerManager;
    class Timer : public std::enable_shared_from_this<Timer>
    {
        friend class TimerManager;

    public:
        typedef std::shared_ptr<Timer> ptr;

        bool Cancel();
        bool Refresh();
        bool Reset(uint64_t ms, bool from_now);

    private:
        Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);
        Timer(uint64_t next);

    private:
        bool mRecurring = false;
        uint64_t mMs = 0;   /// 执行周期
        uint64_t mNext = 0; /// 精确的执行时间
        std::function<void()> mCb;
        TimerManager *mManager = nullptr;

    private:
        struct Comparator
        {
            bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
        };
    };

    class TimerManager
    {
        friend class Timer;

    public:
        typedef RWMutex RWMutexType;

        TimerManager();
        virtual ~TimerManager();

        Timer::ptr AddTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);
        Timer::ptr AddConditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

        uint64_t GetNextTimer();
        void ListExpiredCb(std::vector<std::function<void()>> &cbs);
        bool HasTiemr();

    protected:
        virtual void OnTimeInsertedAtFront() = 0;
        void AddTimer(Timer::ptr val, RWMutexType::WriteLock &lock);

    private:
        bool DelectClockRollover(uint64_t now_ms);

    private:
        RWMutexType mMutex;
        std::set<Timer::ptr, Timer::Comparator> mTimers;
        bool mTickled = false;
        uint64_t mPreviouseTime;
    };
}

#endif