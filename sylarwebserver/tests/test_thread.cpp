#include "sylar/sylar.h"
#include <unistd.h>
sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int volatile count = 0;
sylar::Mutex s_mutex;
void fun1()
{
    SYLAR_LOG_INFO(g_logger) << "name: " << sylar::Thread::S_GetName()
                             << " this.name: " << sylar::Thread::S_GetThis()->GetName()
                             << " id: " << sylar::UtilGetThreadId()
                             << " this.id: " << sylar::Thread::S_GetThis()->GetId();
    // sleep(200);
    for (int i = 0; i < 1000000; i++)
    {
        sylar::Mutex::MutexLock it(s_mutex);
        count++;
    }
}
void fun2()
{
    while (true)
    {

        SYLAR_LOG_INFO(g_logger) << "++++++++++++++++++++++++++++++++++++++++++++";
    }
}
void fun3()
{
    while (true)
    {

        SYLAR_LOG_INFO(g_logger) << "============================================";
    }
}

int main(int argc, char **argv)
{
    SYLAR_LOG_INFO(g_logger) << "thread test begin";
    YAML::Node root = YAML::LoadFile("/home/dcy/work/sylarwebserver/bin/conf/log2.yml");
    sylar::Config::LoadFromYaml(root);
    std::vector<sylar::Thread::ptr> thrs;
    for (int i = 0; i < 2; i++)
    {
        sylar::Thread::ptr thr(new sylar::Thread(&fun1, "name_" + std::to_string(i * 2)));
        // sylar::Thread::ptr thr2(new sylar::Thread(&fun2, "name_" + std::to_string(i * 2 + 1)));
        thrs.push_back(thr);
        // thrs.push_back(thr2);
        //  SYLAR_LOG_INFO(g_logger) << "线程创建成功";
    }

    for (size_t i = 0; i < thrs.size(); i++)
    {
        thrs[i]->Join();
    }
    SYLAR_LOG_INFO(g_logger) << "thread test end";
    SYLAR_LOG_INFO(g_logger) << "count = " << count;

    return 0;
}