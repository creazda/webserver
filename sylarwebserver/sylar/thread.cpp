#include "sylar/thread.h"
#include "sylar/log.h"
#include "sylar/util.h"

namespace sylar
{

    static thread_local Thread *t_thread = nullptr;
    static thread_local std::string t_thread_name = "UNKOWN";

    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Thread *Thread::S_GetThis()
    {
        return t_thread;
    }
    const std::string &Thread::S_GetName()
    {
        return t_thread_name;
    }
    void Thread::S_SetName(const std::string &name)
    {
        if (t_thread)
        {
            t_thread->mName = name;
        }
        t_thread_name = name;
    }

    // 执行函数与线程名
    Thread::Thread(std::function<void()> cb, const std::string &name)
        : mCb(cb), mName(name)
    {
        if (name.empty())
        {
            mName = "UNKOWN";
        }
        //  thread：这是一个指向 pthread_t 类型变量的指针，这个变量用于存储新创建的线程的线程 ID。
        //  attr：这是一个指向 pthread_attr_t 类型变量的指针，用于设置新线程的属性，例如堆栈大小等。如果这个参数为 NULL，那么会使用默认的线程属性。
        //  start_routine：这是一个函数指针，它指向的函数是新线程将要执行的函数。这个函数应该没有返回值，并且接受一个 void* 类型的参数。
        //  arg：这是传递给 start_routine 函数的参数。
        int rt = pthread_create(&mThread, nullptr, &Thread::Run, this); // 创建线程，成功返回0；
        if (rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "pthread_creat thread fail, rt=" << rt << " name" << name;
            throw std::logic_error("pthread_creat error");
        }
        mSemaphore.wait();
    }
    Thread::~Thread()
    {
        if (mThread)
        {
            pthread_detach(mThread);
        }
    }

    void Thread::Join()
    {
        if (mThread)
        {
            int rt = pthread_join(mThread, nullptr); // 成功返回0，否则返回错误码
            if (rt)
            {
                SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt << " name=" << mName;
                throw std::logic_error("pthread_join error");
            }
            mThread = 0;
        }
    }
    // 静态函数获取this需要显示传入
    void *Thread::Run(void *arg)
    {
        Thread *thread = (Thread *)arg;
        t_thread = thread;
        t_thread_name = thread->mName;
        thread->mId = sylar::UtilGetThreadId();
        // 给线程设置一个可读的名称，但是只有16位
        pthread_setname_np(pthread_self(), thread->mName.substr(0, 15).c_str());

        std::function<void()> cb;
        cb.swap(thread->mCb);

        thread->mSemaphore.notify(); // 完成对thread对象的属性初始化之后才允许构造成功
        cb();
        return 0;
    }

}