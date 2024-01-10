#include "fd_manager.h"
#include "hook.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
namespace sylar
{
    FdCtx::FdCtx(int fd)
        : mIsInit(false), mIsSocket(false), mSysNonblock(false), mUserNonblock(false), mIsClosed(false), mFd(fd), mRecvTimeout(-1), mSendTimeout(-1)
    {
        Init();
    }

    FdCtx::~FdCtx()
    {
    }

    bool FdCtx::Init()
    {
        if (mIsInit)
        {
            return true;
        }
        mRecvTimeout = -1;
        mSendTimeout = -1;

        struct stat fd_stat;
        if (-1 == fstat(mFd, &fd_stat))
        {
            mIsInit = false;
            mIsSocket = false;
        }
        else
        {
            mIsInit = true;
            mIsSocket = S_ISSOCK(fd_stat.st_mode);
        }
        if (mIsSocket)
        {
            int flags = fcntl_f(mFd, F_GETFL, 0);
            if (!(flags & O_NONBLOCK))
            {
                fcntl_f(mFd, F_SETFL, flags | O_NONBLOCK);
            }
            mSysNonblock = true;
        }
        else
        {
            mSysNonblock = false;
        }
        mUserNonblock = false;
        mIsClosed = false;
        return mIsInit;
    }

    void FdCtx::SetTImeout(int type, uint64_t v)
    {
        if (type == SO_RCVTIMEO)
        {
            mRecvTimeout = v;
        }
        else
        {
            mSendTimeout = v;
        }
    }
    uint64_t FdCtx::GetTimeout(int type)
    {
        if (type == SO_RCVTIMEO)
        {
            return mRecvTimeout;
        }
        else
        {
            return mSendTimeout;
        }
    }

    FdManager::FdManager()
    {
        mDatas.resize(64);
    }
    FdCtx::ptr FdManager::Get(int fd, bool auto_create)
    {
        RWMutexType::ReadLock lock(mMutex);
        if ((int)mDatas.size() <= fd)
        {
            if (auto_create == false)
            {
                return nullptr;
            }
        }
        else
        {
            if (mDatas[fd] || !auto_create)
            {
                return mDatas[fd];
            }
        }
        lock.UnLock();

        RWMutexType::WriteLock lock2(mMutex);
        FdCtx::ptr ctx(new FdCtx(fd));
        mDatas[fd] = ctx;
        return ctx;
    }
    void FdManager::Del(int fd)
    {
        RWMutexType::WriteLock lock(mMutex);
        if ((int)mDatas.size() <= fd)
        {
            return;
        }
        mDatas[fd].reset();
    }
} // namespace sylar
