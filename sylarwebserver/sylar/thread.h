#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__
#include <pthread.h>
#include <thread>
#include <functional>
#include <memory>
#include <string>
#include "sylar/mutex.h"

namespace sylar
{
    class Thread : Noncopyable
    {

    public:
        typedef std::shared_ptr<Thread> ptr;
        Thread(std::function<void()> cb, const std::string &name);
        ~Thread();

        pid_t GetId() const { return mId; }
        const std::string &GetName() const { return mName; }

        void Join();

        static Thread *S_GetThis();
        static const std::string &S_GetName();
        static void S_SetName(const std::string &name);

    private:
  

        static void *Run(void *arg);

    private:
        pid_t mId = -1;
        pthread_t mThread = 0;
        std::function<void()> mCb;
        std::string mName;

        Semaphore mSemaphore;
    };

}
#endif // __SYLAR_THREAD_H__