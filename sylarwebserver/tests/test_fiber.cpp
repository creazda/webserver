#include "sylar/sylar.h"

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void run_in_fiber()
{
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber begin";
    sylar::Fiber::YieldToHold();
    SYLAR_LOG_INFO(g_logger) << "run_in_fiber end";
    sylar::Fiber::YieldToHold();
}

void test_fiber()
{
    SYLAR_LOG_INFO(g_logger) << "main start 1";

    {

        sylar::Fiber::GetThis();
        SYLAR_LOG_INFO(g_logger) << "main start";
        sylar::Fiber::ptr fiber(new sylar::Fiber(run_in_fiber));
        fiber->SwapIn();
        SYLAR_LOG_INFO(g_logger) << "main after swapin";
        fiber->SwapIn();
        SYLAR_LOG_INFO(g_logger) << "main end";
        fiber->SwapIn();
    }
    SYLAR_LOG_INFO(g_logger) << "main end  2";
}
int main(int argc, const char **argv)
{
    sylar::Thread::S_SetName("main");
    std::vector<sylar::Thread::ptr> thrs;
    for (int i = 0; i < 3; i++)
    {
        thrs.push_back(sylar::Thread::ptr(new sylar::Thread(&test_fiber, "test_fiber " + std::to_string(i))));
    }
    for (int i = 0; i < 3; i++)
    {
        thrs[i]->Join();
    }

    return 0;
}