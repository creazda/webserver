#include "sylar/iomanager.h"
#include "sylar/macro.h"
#include "sylar/log.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <error.h>
namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    IOManager::FdContext::EventContext &IOManager::FdContext::GetContext(IOManager::Event event)
    {
        switch (event)
        {
        case IOManager::READ:
            return read;
            break;
        case IOManager::WRITE:
            return write;
        default:
            SYLAR_ASSERT2(false, "GetContext");
        }
    }
    void IOManager::FdContext::ResetContext(EventContext &ctx)
    {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }
    void IOManager::FdContext::TriggerEvent(Event event)
    {
        SYLAR_ASSERT(events & event);
        events = (Event)(events & ~event); // 删掉
        EventContext &ctx = GetContext(event);
        if (ctx.cb)
        {
            ctx.scheduler->schedule(&ctx.cb);
        }
        else
        {
            ctx.scheduler->schedule(&ctx.fiber);
        }
        ctx.scheduler = nullptr;
        return;
    }

    IOManager::IOManager(size_t threads, bool use_caller, const std::string &name)
        : Scheduler(threads, use_caller, name)
    {
        mEpfd = epoll_create(5000);
        SYLAR_ASSERT(mEpfd > 0);

        // std::cout << "iomanager" << std::endl;
        int rt = pipe(mTickleFds);
        SYLAR_ASSERT(rt == 0);

        epoll_event event;
        memset(&event, 0, sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET; // 可读，边缘触发
        event.data.fd = mTickleFds[0];    // 链接管道的读文件描述符

        rt = fcntl(mTickleFds[0], F_SETFL, O_NONBLOCK); // 设置管道读文件描述符非阻塞
        SYLAR_ASSERT(!rt);

        rt = epoll_ctl(mEpfd, EPOLL_CTL_ADD, mTickleFds[0], &event); // 添加对mTickleFds[0]的监听
        SYLAR_ASSERT(!rt);

        ContextResize(32);
        Start();
    }
    IOManager::~IOManager()
    {
        Stop();
        close(mEpfd);
        close(mTickleFds[0]);
        close(mTickleFds[1]);

        for (size_t i = 0; i < mFdContexts.size(); i++)
        {
            if (mFdContexts[i])
            {
                delete mFdContexts[i];
            }
        }
    }

    void IOManager::ContextResize(size_t size)
    {
        mFdContexts.resize(size);

        for (size_t i = 0; i < mFdContexts.size(); i++)
        {
            if (!mFdContexts[i])
            { // 若为空
                mFdContexts[i] = new FdContext;
                mFdContexts[i]->fd = i;
            }
        }
    }

    int IOManager::AddEvent(int fd, Event event, std::function<void()> cb)
    {
        SYLAR_LOG_INFO(g_logger) << "AddEvent()";
        FdContext *fd_ctx = nullptr;
        RWMutexType::ReadLock lock(mMutex);
        if ((int)mFdContexts.size() > fd)
        {
            fd_ctx = mFdContexts[fd];
            lock.UnLock();
        }
        else
        {
            lock.UnLock();
            RWMutexType::WriteLock lock2(mMutex);
            ContextResize(fd * 1.5);
            fd_ctx = mFdContexts[fd];
        }

        FdContext::MutexType::MutexLock lock2(fd_ctx->mutex);
        if (fd_ctx->events & event)
        { // 如果进入该区域，那么至少有两个不同的进程在操作同一个句柄的用一个方法
            SYLAR_LOG_ERROR(g_logger) << "addEvent assert fd=" << fd
                                      << " event=" << event
                                      << " fd_ctx.event" << fd_ctx->events;
            SYLAR_ASSERT(!(fd_ctx->events & event));
        }

        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD; // 已存在就是mod，未存在就是add
        epoll_event epevent;
        epevent.events = EPOLLET | fd_ctx->events | event;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(mEpfd, op, fd, &epevent);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << mEpfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "):"
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return -1;
        }

        ++mPendingEventCount;
        fd_ctx->events = (Event)(fd_ctx->events | event);
        FdContext::EventContext &event_ctx = fd_ctx->GetContext(event);
        SYLAR_ASSERT(!event_ctx.cb && !event_ctx.fiber && !event_ctx.scheduler); // 断言全部为空，因为是刚加入的event

        event_ctx.scheduler = Scheduler::GetThis();
        if (cb)
        {
            event_ctx.cb.swap(cb);
        }
        else
        {
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT2(event_ctx.fiber->GetState() == Fiber::EXEC, "state=" << event_ctx.fiber->GetState());
        }
        return 0;
    }
    bool IOManager::DelEvent(int fd, Event event)
    {
        RWMutexType::ReadLock lock(mMutex);
        if ((int)mFdContexts.size() <= fd)
        { // 删除的fd不存在
            return false;
        }
        FdContext *fd_ctx = mFdContexts[fd];
        lock.UnLock();

        FdContext::MutexType::MutexLock lock2(fd_ctx->mutex);
        if (!(fd_ctx->events & event))
        { // 删除的事件不存在
            return false;
        }

        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL; // 只删除几项还是全部删除
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(mEpfd, op, fd, &epevent);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << mEpfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "):"
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        --mPendingEventCount;
        fd_ctx->events = new_events;
        FdContext::EventContext &event_ctx = fd_ctx->GetContext(event);
        fd_ctx->ResetContext(event_ctx);
        return true;
    }
    bool IOManager::CancelEvent(int fd, Event event)
    {
        RWMutexType::ReadLock lock(mMutex);
        if ((int)mFdContexts.size() <= fd)
        { // 删除的fd不存在
            return false;
        }
        FdContext *fd_ctx = mFdContexts[fd];
        lock.UnLock();

        FdContext::MutexType::MutexLock lock2(fd_ctx->mutex);
        if (!(fd_ctx->events & event))
        { // 删除的事件不存在
            return false;
        }

        Event new_events = (Event)(fd_ctx->events & ~event);
        int op = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL; // 只删除几项还是全部删除
        epoll_event epevent;
        epevent.events = EPOLLET | new_events;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(mEpfd, op, fd, &epevent);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << mEpfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "):"
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        fd_ctx->TriggerEvent(event);
        --mPendingEventCount;

        return true;
    }

    bool IOManager::CancelAll(int fd)
    {
        RWMutexType::ReadLock lock(mMutex);
        if ((int)mFdContexts.size() <= fd)
        { // 删除的fd不存在
            return false;
        }
        FdContext *fd_ctx = mFdContexts[fd];
        lock.UnLock();

        FdContext::MutexType::MutexLock lock2(fd_ctx->mutex);
        if (!fd_ctx->events)
        { // 删除的事件不存在
            return false;
        }

        int op = EPOLL_CTL_DEL; // 只删除几项还是全部删除
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr = fd_ctx;

        int rt = epoll_ctl(mEpfd, op, fd, &epevent);
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << mEpfd << ", "
                                      << op << ", " << fd << ", " << epevent.events << "):"
                                      << rt << " (" << errno << ") (" << strerror(errno) << ")";
            return false;
        }

        if (fd_ctx->events & READ)
        {
            fd_ctx->TriggerEvent(READ);
            --mPendingEventCount;
        }
        if (fd_ctx->events & WRITE)
        {
            fd_ctx->TriggerEvent(WRITE);
            --mPendingEventCount;
        }
        SYLAR_ASSERT(fd_ctx->events == 0);
        return true;
    }

    IOManager *IOManager::GetThis()
    {
        return dynamic_cast<IOManager *>(Scheduler::GetThis());
    }

    void IOManager::Tickle()
    {
        SYLAR_LOG_INFO(g_logger) << "子类tickle";
        if (!HasIdleThreads())
        {
            SYLAR_LOG_DEBUG(g_logger) << "暂时没有Idle";
            return;
        }
        SYLAR_LOG_INFO(g_logger) << "拥有idle线程，向管道发送信息";
        int rt = write(mTickleFds[1], "T", 1);
        SYLAR_ASSERT(rt == 1);
    }
    bool IOManager::Stopping()
    {
        uint64_t timeout = 0;
        return Stopping(timeout);
    }
    bool IOManager::Stopping(uint64_t &timeout)
    {
        timeout = GetNextTimer();
        return timeout == ~0ull && mPendingEventCount == 0 && Scheduler::Stopping();
    }
    void IOManager::Idle()
    {
        epoll_event *events = new epoll_event[64]();
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr)
                                                   { delete[] ptr; }); // 自定义删除器
        while (true)
        {
            uint64_t next_timeout = 0;
            if (Stopping(next_timeout))
            {

                SYLAR_LOG_INFO(g_logger) << "name=" << GetName() << " idle stopping exit";
                break;
            }

            int rt = 0;
            do
            {
                SYLAR_LOG_INFO(g_logger) << "监听注册的事件";
                static const int MAX_TIMEOUT = 3000;
                if (next_timeout != ~0ull)
                { // 还有任务
                    next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
                }
                else
                {
                    next_timeout = MAX_TIMEOUT;
                }
                SYLAR_LOG_DEBUG(g_logger) << "next_timeout:" << next_timeout;
                rt = epoll_wait(mEpfd, events, 64, (int)next_timeout);

                if (rt < 0 && errno == EINTR) // 中断就继续，否则就break
                {
                }
                else
                { // 成功或者其他错误就break
                    SYLAR_LOG_INFO(g_logger) << "监听成功,rt=" << rt;
                    break;
                }
            } while (true);
            std::vector<std::function<void()>> cbs;
            ListExpiredCb(cbs);
            if (!cbs.empty())
            {
                SYLAR_LOG_DEBUG(g_logger) << "on timer cbs.size=" << cbs.size();
                schedule(cbs.begin(), cbs.end());
                cbs.clear();
            }

            for (int i = 0; i < rt; i++)
            {
                epoll_event &event = events[i];
                if (event.data.fd == mTickleFds[0])
                { // 如果是对管道的监听，通过while读出来
                    SYLAR_LOG_INFO(g_logger) << "检测到管道触发事件";
                    uint8_t dummy;
                    while (read(mTickleFds[0], &dummy, 1) == 1)
                        ;
                    continue;
                }
                //
                FdContext *fd_ctx = (FdContext *)event.data.ptr;
                FdContext::MutexType::MutexLock lock(fd_ctx->mutex);
                if (event.events & (EPOLLERR | EPOLLHUP)) // 读写错误或者链接关闭
                {
                    event.events |= EPOLLIN | EPOLLOUT;
                }
                int real_events = NONE;
                if (event.events & EPOLLIN)
                {
                    real_events = READ;
                }
                if (event.events & EPOLLOUT)
                {
                    real_events = WRITE;
                }
                if ((fd_ctx->events & real_events) == NONE)
                { // 无事件
                    continue;
                }

                int left_events = (fd_ctx->events & ~real_events); // 删除掉读写事件
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                event.events = EPOLLET | left_events;
                int rt2 = epoll_ctl(mEpfd, op, fd_ctx->fd, &event);
                if (rt2)
                {
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << mEpfd << ", "
                                              << op << ", " << fd_ctx->fd << ", " << event.events << "):"
                                              << rt << " (" << errno << ") (" << strerror(errno) << ")";
                    continue;
                }
                if (real_events & READ)
                {
                    fd_ctx->TriggerEvent(READ);
                    --mPendingEventCount;
                }
                if (real_events & WRITE)
                {
                    fd_ctx->TriggerEvent(WRITE);
                    --mPendingEventCount;
                }
            }
            Fiber::ptr cur = Fiber::GetThis();
            auto raw_ptr = cur.get();
            cur.reset();
            raw_ptr->SwapOut();
        }
    }
    void IOManager::OnTimeInsertedAtFront()
    {
        Tickle();
    }
}