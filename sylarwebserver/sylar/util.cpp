#include "util.h"
#include <execinfo.h>
#include "sylar/log.h"
#include "sylar/fiber.h"
#include <sys/time.h>
namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    pid_t UtilGetThreadId()
    {
        return syscall(SYS_gettid);
    }
    u_int32_t UtilGetFiberId()
    {
        return sylar::Fiber::GetFiberId();
    }

    void Backtrace(std::vector<std::string> &bt, int size, int skip)
    {
        // 一般是void* array[size] 但是协程的栈本来就小，通过malloc放到堆上
        void **array = (void **)malloc((sizeof(void *) * size));
        size_t s = ::backtrace(array, size);
        char **strings = backtrace_symbols(array, s);
        if (strings == NULL)
        {
            SYLAR_LOG_INFO(g_logger) << "backtrace_synbols error";
            free(strings);
            free(array);
            return;
        }
        for (size_t i = skip; i < s; i++)
        {
            bt.push_back(strings[i]);
        }
        free(strings);
        free(array);
    }
    std::string BacktraceToString(int size, int skip, const std::string &prefix)
    {
        std::vector<std::string> bt;
        Backtrace(bt, size, skip);
        std::stringstream ss;
        for (size_t i = 0; i < bt.size(); i++)
        {
            ss << prefix << bt[i] << std::endl;
        }
        return ss.str();
    }
    // tv_sec为秒，tv_usec为微秒。
    uint64_t GetCurrentMS()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
    }
    uint64_t GetCurrentUS()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
    }

}