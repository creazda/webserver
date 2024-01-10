#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_func()
{
    SYLAR_LOG_INFO(g_logger) << "test func";
    sleep(1);
    static int s_count = 5;
    if (--s_count >= 0)
    {
        sylar::Scheduler::GetThis()->schedule(&test_func, sylar::UtilGetThreadId());
    }
}
int main(int argc, const char **argv)
{
    SYLAR_LOG_INFO(g_logger) << "begin";

    sylar::Scheduler sc(3, true);
    sc.schedule(&test_func);
    sc.Start();

    sc.Stop();
    SYLAR_LOG_INFO(g_logger) << "over";
    return 0;
}