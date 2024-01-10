#include "timer.h"
#include "util.h"
namespace sylar
{

    bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const
    {
        if (!lhs && !rhs)
        {
            return false;
        }
        if (!lhs)
        {
            return true;
        }
        if (!rhs)
        {
            return false;
        }
        if (lhs->mNext < rhs->mNext)
        {
            return true;
        }
        if (lhs->mNext > rhs->mNext)
        {
            return false;
        }
        return lhs.get() < rhs.get();
    }
    Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager)
        : mRecurring(recurring), mMs(ms), mCb(cb), mManager(manager)
    {
        mNext = sylar::GetCurrentMS() + mMs;
    }
    Timer::Timer(uint64_t next)
        : mNext(next)
    {
    }

    bool Timer::Cancel()
    {
        TimerManager::RWMutexType::WriteLock lock(mManager->mMutex);
        if (mCb)
        {
            mCb = nullptr;
            auto it = mManager->mTimers.find(shared_from_this());
            mManager->mTimers.erase(it);
            return true;
        }
        return false;
    }
    bool Timer::Refresh()
    {
        TimerManager::RWMutexType::WriteLock lock(mManager->mMutex);

        if (!mCb)
        {
            return false;
        }
        auto it = mManager->mTimers.find(shared_from_this());
        if (it == mManager->mTimers.end())
        {
            return false;
        }
        mManager->mTimers.erase(it);
        mNext = sylar::GetCurrentMS() + mMs;
        mManager->mTimers.insert(shared_from_this());
        return true;
    }
    bool Timer::Reset(uint64_t ms, bool from_now)
    {
        if (ms == mMs && !from_now)
        { // 如果ms相同，且不需要从现在就开始，则直接返回true
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(mManager->mMutex);

        if (!mCb)
        {
            return false;
        }
        auto it = mManager->mTimers.find(shared_from_this());
        if (it == mManager->mTimers.end())
        {
            return false;
        }
        mManager->mTimers.erase(it);
        uint64_t start = 0;
        if (from_now)
        {
            start = sylar::GetCurrentMS();
        }
        else
        {
            start = mNext - mMs;
        }
        mMs = ms;
        mNext = start + mMs;
        mManager->AddTimer(shared_from_this(), lock);
        return true;
    }

    TimerManager::TimerManager()
    {
        mPreviouseTime = sylar::GetCurrentMS();
    }
    TimerManager::~TimerManager()
    {
    }

    Timer::ptr TimerManager::AddTimer(uint64_t ms, std::function<void()> cb, bool recurring)
    {
        Timer::ptr timer(new Timer(ms, cb, recurring, this));
        RWMutexType::WriteLock lock(mMutex);
        AddTimer(timer, lock);
        return timer;
    }

    static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
    {
        std::shared_ptr<void> tmp = weak_cond.lock();
        if (tmp)
        {
            cb();
        }
    }
    Timer::ptr TimerManager::AddConditionTimer(uint64_t ms, std::function<void()> cb,
                                               std::weak_ptr<void> weak_cond, bool recurring)
    {
        return AddTimer(ms, std::bind(&OnTimer, weak_cond, cb), recurring);
    }

    uint64_t TimerManager::GetNextTimer()
    {
        RWMutexType::ReadLock lock(mMutex);
        mTickled = false;
        if (mTimers.empty())
        {
            return ~0ull;
        }
        const Timer::ptr next = *mTimers.begin();
        uint64_t now_ms = sylar::GetCurrentMS();
        if (now_ms >= next->mNext)
        { // 说明定时器应该执行了，但是晚了
            return 0;
        }
        else
        {
            return next->mNext - now_ms; // 返回还要等待的时间
        }
    }
    // 选取需要执行的timer
    void TimerManager::ListExpiredCb(std::vector<std::function<void()>> &cbs)
    {
        uint64_t now_ms = sylar::GetCurrentMS();
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(mMutex);
            if (mTimers.empty())
            {
                return;
            }
        }
        RWMutexType::WriteLock lock(mMutex);

        if (mTimers.empty())
        {
            return;
        }
        bool rollover = DelectClockRollover(now_ms);
        if (!rollover && (*mTimers.begin())->mNext > now_ms)
        { // 如果时间上没错误，且最近的timer也没超时，直接返回
            return;
        }
        Timer::ptr now_timer(new Timer(now_ms));
        auto it = rollover ? mTimers.end() : mTimers.lower_bound(now_timer);
        while (it != mTimers.end() && (*it)->mNext == now_ms)
        { // 找到最后一个超时的
            ++it;
        }
        expired.insert(expired.begin(), mTimers.begin(), it);
        mTimers.erase(mTimers.begin(), it);
        cbs.reserve(expired.size());

        for (auto &timer : expired)
        {
            cbs.push_back(timer->mCb);
            if (timer->mRecurring)
            {
                timer->mNext = now_ms + timer->mMs;
                mTimers.insert(timer);
            }
            else
            {
                timer->mCb = nullptr;
            }
        }
    }
    void TimerManager::AddTimer(Timer::ptr val, RWMutexType::WriteLock &lock)
    {
        auto it = mTimers.insert(val).first;
        bool at_front = (it == mTimers.begin()) && !mTickled;
        if (at_front)
        {
            mTickled = true;
        }
        lock.UnLock();
        if (at_front)
        {
            OnTimeInsertedAtFront();
        }
    }
    bool TimerManager::DelectClockRollover(uint64_t now_ms)
    {
        bool rollover = false;
        if (now_ms < mPreviouseTime &&
            now_ms < (mPreviouseTime - 60 * 60 * 1000))
        {
            rollover = true;
        }
        mPreviouseTime = now_ms;
        return rollover;
    }
    bool TimerManager::HasTiemr()
    {
        RWMutexType::ReadLock lock(mMutex);
        return !mTimers.empty();
    }
}