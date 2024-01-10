#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__

#include "sylar/scheduler.h"
#include "sylar/mutex.h"
#include "sylar/timer.h"

namespace sylar
{
    class IOManager : public Scheduler, public TimerManager
    {
    public:
        typedef std::shared_ptr<IOManager> ptr;
        typedef RWMutex RWMutexType;

        enum Event
        {
            NONE = 0x0,
            READ = 0x1,  /// 读事件(EPOLLIN) 为 0001
            WRITE = 0x4, /// 写事件(EPOLLOUT)为 0100
        };

    private:
        struct FdContext // socket 事件上下文类
        {
            typedef Mutex MutexType;
            struct EventContext // 事件上下文类
            {
                Scheduler *scheduler = nullptr; // 事件执行的scheduler
                Fiber::ptr fiber;               // 事件协程
                std::function<void()> cb;       // 事件的回调函数
            };

            EventContext &GetContext(Event event);
            void ResetContext(EventContext &ctx);
            void TriggerEvent(Event event);
            EventContext read;   // 读事件
            EventContext write;  // 写事件
            int fd;              // 事件关联的句柄
            Event events = NONE; // 已注册的事件
            MutexType mutex;
        };

    public:
        IOManager(size_t threads = 1, bool use_caller = true, const std::string &name = "");
        ~IOManager();

        int AddEvent(int fd, Event event, std::function<void()> cb = nullptr);
        bool DelEvent(int fd, Event event);
        bool CancelEvent(int fd, Event event);

        bool CancelAll(int fd);

        static IOManager *GetThis();

    protected:
        void Tickle() override;
        bool Stopping() override;
        bool Stopping(uint64_t &timeout);
        void Idle() override;
        void OnTimeInsertedAtFront() override;
        void ContextResize(size_t size);

    private:
        int mEpfd = 0;     // epoll文件句柄
        int mTickleFds[2]; // pipe文件句柄

        std::atomic<size_t> mPendingEventCount = {0}; // 当前等待执行的事件数量
        RWMutexType mMutex;
        std::vector<FdContext *> mFdContexts; // socket事件的上下文容器
    };
}

#endif // !__SYLAR_IOMANAGER_H__
