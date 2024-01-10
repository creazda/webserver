#include "hook.h"
#include <dlfcn.h>
#include "scheduler.h"
#include "fiber.h"
#include "iomanager.h"
#include "fd_manager.h"
#include "config.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

namespace sylar
{

    static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout =
        sylar::Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");
    static thread_local bool t_hook_enable = false;

#define HOOK_FUN(XX) \
    XX(sleep)        \
    XX(usleep)       \
    XX(nanosleep)    \
    XX(socket)       \
    XX(connect)      \
    XX(accept)       \
    XX(read)         \
    XX(readv)        \
    XX(recv)         \
    XX(recvfrom)     \
    XX(recvmsg)      \
    XX(write)        \
    XX(writev)       \
    XX(send)         \
    XX(sendto)       \
    XX(sendmsg)      \
    XX(close)        \
    XX(fcntl)        \
    XX(ioctl)        \
    XX(getsockopt)   \
    XX(setsockopt)

    void hook_init()
    {
        static bool is_inited = false;
        if (is_inited)
        {
            return;
        }
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
        HOOK_FUN(XX);
#undef XX
        // 使用RTLD_NEXT参数找到的的函数指针就是后面第一次出现这个函数名的函数指针。大致意思就是说我们可能会链接多个动态库，不同的动态库可能都会有symbol这个函数名，那么使用RTLD_NEXT参数后dlsym返回的就是第一个遇到(匹配上)symbol这个符号的函数的函数地址。进一步的我们使用dlsym的返回调用的也就是这个第一个匹配上的函数了。
    }

    static uint64_t s_connect_timeout = -1;
    // 在main执行之前初始化
    struct _HookIniter
    {
        _HookIniter()
        {
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->GetValue();

            g_tcp_connect_timeout->AddListener([](const int &old_value, const int &new_value)
                                               {    
                                                SYLAR_LOG_INFO(g_logger) <<  "tcp connect timeout changed from " 
                                                << old_value << " to " << new_value;
                                                s_connect_timeout = new_value; });
        }
    };
    static _HookIniter s_hook_initer;

    bool is_hook_enable()
    {
        return t_hook_enable;
    }
    void set_hook_enable(bool flag)
    {
        t_hook_enable = flag;
    }
}

struct timer_info
{
    int cancelled = 0;
};

template <typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name, uint32_t event,
                     int timeout_so, Args &&...args)
{
    if (!sylar::t_hook_enable)
    {
        return fun(fd, std::forward<Args>(args)...); // std::forward用于将参数的完整特性传递给模板中需要的地方
    }
    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
    if (!ctx)
    {
        return fun(fd, std::forward<Args>(args)...);
    }
    if (ctx->IsClose())
    {
        errno = EBADF; // 错误的文件描述符
        return -1;
    }

    if (!ctx->IsSocket() || ctx->GetUserNonblock())
    { // 不是socket或者是用户非阻塞
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->GetTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) // 错误表示任务被中断
    {
        n = fun(fd, std::forward<Args>(args)...);
    }
    // SYLAR_LOG_DEBUG(g_logger) << "do_io <" << hook_fun_name << ">";

    if (n == -1 && errno == EAGAIN) // 错误表示任务无法立即完成,这时需要处理，否则直接返回
    {
        sylar::IOManager *iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo); // 条件定时器的条件

        if (to != (uint64_t)-1)
        { // 说明有设置超时
            timer = iom->AddConditionTimer(
                to, [winfo, fd, iom, event]() { // 超时则触发
                    auto t = winfo.lock();
                    if (!t || t->cancelled)
                    { // 条件无
                        return;
                    }
                    t->cancelled = ETIMEDOUT; // 设置错误码，在给定的时间没有收到回馈
                    iom->CancelEvent(fd, (sylar::IOManager::Event)(event));
                },
                winfo);
        }

        int rt = iom->AddEvent(fd, (sylar::IOManager::Event)(event));
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << hook_fun_name << "addevent(" << fd << ", " << event << ")";
            if (timer)
            {
                timer->Cancel();
            }
            return -1;
        }
        else
        { // 注册成功的话就让出cpu
            sylar::Fiber::YieldToHold();
            // 返回有两种情况:一种是io事件完成，另一种是定时器超时。
            if (timer)
            {
                timer->Cancel();
            }
            if (tinfo->cancelled)
            { // 说明是通过定时任务唤醒的
                errno = tinfo->cancelled;
                return -1;
            }
            // SYLAR_LOG_DEBUG(g_logger) << "goto retry";
            goto retry;
        }
    }
    return n;
}

extern "C"
{
#define XX(name) name##_fun name##_f = nullptr;
    HOOK_FUN(XX);
#undef XX

    unsigned int sleep(unsigned int seconds)
    {
        if (!sylar::t_hook_enable)
        { // 不hook的话就直接用原来的sleep
            return sleep_f(seconds);
        }
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager *iom = sylar::IOManager::GetThis();
        // iom->AddTimer(seconds * 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
        iom->AddTimer(seconds * 1000, std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int threadid)) & sylar::IOManager::schedule, iom, fiber, -1)); // schedule为模板函数，bind的时候需要把参数传进去
        sylar::Fiber::YieldToHold();
        return 0;
    }
    int usleep(useconds_t usec)
    {
        if (!sylar::t_hook_enable)
        { // 不hook的话就直接用原来的sleep
            return usleep_f(usec);
        }
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager *iom = sylar::IOManager::GetThis();
        // iom->AddTimer(usec / 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
        iom->AddTimer(usec / 1000, std::bind((void(sylar::Scheduler::*)(sylar::Fiber::ptr, int threadid)) & sylar::Scheduler::schedule, iom, fiber, -1));
        sylar::Fiber::YieldToHold();
        return 0;
    }

    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        if (!sylar::t_hook_enable)
        { // 不hook的话就直接用原来的sleep
            return nanosleep_f(req, rem);
        }

        int timeout_ms = req->tv_sec * 1000 + req->tv_nsec / 1000 / 1000;
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager *iom = sylar::IOManager::GetThis();
        // iom->AddTimer(usec / 1000, std::bind(&sylar::IOManager::schedule, iom, fiber));
        iom->AddTimer(timeout_ms, [iom, fiber]()
                      { iom->schedule(fiber); });
        sylar::Fiber::YieldToHold();
        return 0;
    }

    // socket
    int socket(int domain, int type, int protocol)
    {
        if (!sylar::t_hook_enable)
        {
            return socket_f(domain, type, protocol);
        }
        int fd = socket_f(domain, type, protocol);
        if (fd == -1)
        {
            return fd;
        }
        sylar::FdMgr::GetInstance()->Get(fd, true);
        return fd;
    }

    int connect_with_timeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeout_ms)
    {
        if (!sylar::t_hook_enable)
        {
            return connect_f(fd, addr, addrlen);
        }
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
        if (!ctx || ctx->IsClose())
        { // 错误的文件描述符
            errno = EBADF;
            return -1;
        }

        if (!ctx->IsSocket())
        { // 如果不是sock，直接调用原函数。
            return connect_f(fd, addr, addrlen);
        }

        if (ctx->GetUserNonblock())
        { // 如果已经注册为非阻塞
            return connect_f(fd, addr, addrlen);
        }

        int n = connect_f(fd, addr, addrlen);
        if (n == 0)
        {
            return 0;
        }
        else if (n != -1 || errno != EINPROGRESS) // EINPROGRESS表示尚在进行中，无法立即完成
        {
            return n;
        }

        // 表示既不失败，又没成功；正处于errno = EINPROGRESS 状态
        sylar::IOManager *iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);

        if (timeout_ms != (uint64_t)-1)
        {
            timer = iom->AddConditionTimer(
                timeout_ms, [winfo, fd, iom]()
                {
                auto t = winfo.lock();
                if(!t || t->cancelled)
                {
                    return ;
                } 
                t->cancelled = ETIMEDOUT;
                iom->CancelEvent(fd,sylar::IOManager::WRITE); },
                winfo);
        }

        int rt = iom->AddEvent(fd, sylar::IOManager::WRITE);
        if (rt == 0)
        { // success
            sylar::Fiber::YieldToHold();
            if (timer)
            {
                timer->Cancel();
            }
            if (tinfo->cancelled)
            { // 超时
                errno = tinfo->cancelled;
                return -1;
            }
        }
        else
        { //
            if (timer)
            {
                timer->Cancel();
            }
            SYLAR_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
        }

        int error = 0;
        socklen_t len = sizeof(int);
        if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))
        {
            return -1;
        }
        if (!error)
        {
            return 0;
        }
        else
        {
            errno = error;
            return -1;
        }
    }
    int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
    {
        return connect_with_timeout(sockfd, addr, addrlen, sylar::s_connect_timeout);
    }

    int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
    {
        int fd = do_io(s, accept_f, "accept", sylar::IOManager::READ, SO_RCVTIMEO, addr, addrlen);
        if (fd >= 0)
        {
            sylar::FdMgr::GetInstance()->Get(fd, true);
        }
        return fd;
    }

    // read
    ssize_t read(int fd, void *buf, size_t count)
    {
        return do_io(fd, read_f, "read", sylar::IOManager::READ, SO_RCVTIMEO, buf, count);
    }

    ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, readv_f, "readv", sylar::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return do_io(sockfd, recv_f, "recv", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags);
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
    {
        return do_io(sockfd, recvfrom_f, "recvfrom", sylar::IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
    }

    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
    {
        return do_io(sockfd, recvmsg_f, "recvmsg", sylar::IOManager::READ, SO_RCVTIMEO, msg, flags);
    }

    // write
    ssize_t write(int fd, const void *buf, size_t count)
    {
        return do_io(fd, write_f, "write", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, count);
    }

    ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd, writev_f, "writev", sylar::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
    }

    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        return do_io(sockfd, send_f, "send", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags);
    }

    ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                   const struct sockaddr *dest_addr, socklen_t addrlen)
    {
        return do_io(sockfd, sendto_f, "sendto", sylar::IOManager::WRITE, SO_SNDTIMEO, buf, len, flags, dest_addr, addrlen);
    }

    ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
    {
        return do_io(sockfd, sendmsg_f, "sendmsg", sylar::IOManager::WRITE, SO_SNDTIMEO, msg, flags);
    }

    int close(int fd)
    {
        if (!sylar::t_hook_enable)
        {
            return close_f(fd);
        }
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
        if (ctx)
        {
            auto iom = sylar::IOManager::GetThis();
            if (iom)
            {
                iom->CancelAll(fd);
            }
            sylar::FdMgr::GetInstance()->Del(fd);
        }
        return close_f(fd);
    }
    int fcntl(int fd, int cmd, ... /* arg */)
    {
        // if (!sylar::t_hook_enable)
        // {
        //     return fcntl_f(fd, cmd, ...);
        // }
        va_list va;
        va_start(va, cmd);
        switch (cmd)
        {
        case F_SETFL:
        {
            int arg = va_arg(va, int);
            va_end(va);
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
            if (!ctx || ctx->IsClose() || !ctx->IsSocket())
            { // 如果无ctx或者已经关闭，或者不是socket链接

                return fcntl_f(fd, cmd, arg);
            }
            ctx->SetUserNonblock(arg & O_NONBLOCK);
            if (ctx->GetSysNonblock())
            { // 如果系统设置nonblock,这是欺骗系统的，实际上执行的是nonblock
                arg |= O_NONBLOCK;
            }
            else
            {
                arg &= ~O_NONBLOCK;
            }
            return fcntl_f(fd, cmd, arg);
        }
        break;
        case F_GETFL:
        {
            va_end(va);
            int arg = fcntl_f(fd, cmd);
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
            if (!ctx || ctx->IsClose() || !ctx->IsSocket())
            {
                return arg;
            }
            if (ctx->GetUserNonblock())
            {
                return arg | O_NONBLOCK;
            }
            else
            {
                return arg & ~O_NONBLOCK;
            }
        }
        break;
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
        case F_SETFD:
        case F_SETOWN:
        case F_SETSIG:
        case F_SETLEASE:
        case F_NOTIFY:
        case F_SETPIPE_SZ:
        {
            int arg = va_arg(va, int);
            va_end(va);
            return fcntl(fd, cmd, arg);
        }
        break;
        case F_GETFD:
        case F_GETOWN:
        case F_GETSIG:
        case F_GETLEASE:
        case F_GETPIPE_SZ:
        {
            va_end(va);
            return fcntl(fd, cmd);
        }
        break;
        case F_SETLK:
        case F_SETLKW:
        case F_GETLK:
        {
            struct flock *arg = va_arg(va, struct flock *);
            va_end(va);
            return fcntl(fd, cmd, arg);
        }
        break;
        case F_GETOWN_EX:
        case F_SETOWN_EX:
        {
            struct f_owner_exlock *arg = va_arg(va, struct f_owner_exlock *);
            va_end(va);
            return fcntl(fd, cmd, arg);
        }
        break;
        default:
            va_end(va);
            return fcntl_f(fd, cmd);
            break;
        }
    }

    int ioctl(int fd, unsigned long request, ...)
    {
        va_list va;
        va_start(va, request);
        void *arg = va_arg(va, void *);
        va_end(va);

        if (FIONBIO == request)
        {
            bool user_nonblock = !!*(int *)arg; // 连续两个感叹号一般用来转化为bool值
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(fd);
            if (!ctx || ctx->IsClose() || !ctx->IsSocket())
            { // 无文件描述符或者已经关闭或不是socket
                return ioctl_f(fd, request, arg);
            }
            ctx->SetUserNonblock(user_nonblock);
        }
        return ioctl_f(fd, request, arg);
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
    {
        return getsockopt_f(sockfd, level, optname, optval, optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        if (!sylar::t_hook_enable)
        {
            return setsockopt(sockfd, level, optname, optval, optlen);
        }
        if (level == SOL_SOCKET)
        {
            if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)
            { // 如果实在设置接受和发送的超时时间的话
                sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->Get(sockfd);
                if (ctx)
                {
                    const timeval *v = (const timeval *)optval;
                    ctx->SetTImeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
                }
            }
        }
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
}