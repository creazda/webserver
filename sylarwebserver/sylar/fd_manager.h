#ifndef __FDMANAGER_H__
#define __FDMANAGER_H__
#include <memory>
#include <vector>
#include "thread.h"
#include "iomanager.h"
#include <sys/stat.h>
#include "singleton.h"
namespace sylar
{
    class FdCtx : public std::enable_shared_from_this<FdCtx>
    {
    public:
        typedef std::shared_ptr<FdCtx> ptr;
        FdCtx(int fd);
        ~FdCtx();

        bool Init();
        bool IsInit() const { return mIsInit; }
        bool IsSocket() const { return mIsSocket; }
        bool IsClose() const { return mIsClosed; }
        bool Close();

        void SetUserNonblock(bool v) { mUserNonblock = v; }
        bool GetUserNonblock() const { return mUserNonblock; }

        void SetSysNonblock(bool v) { mSysNonblock = v; }
        bool GetSysNonblock() const { return mSysNonblock; }

        void SetTImeout(int type, uint64_t v);
        uint64_t GetTimeout(int type);

    private:
        bool mIsInit : 1;
        bool mIsSocket : 1;
        bool mSysNonblock : 1;
        bool mUserNonblock : 1;
        bool mIsClosed : 1;
        int mFd;
        uint64_t mRecvTimeout;
        uint64_t mSendTimeout;
        // sylar::IOManager *mIOManager;
    };

    class FdManager
    {
    public:
        typedef RWMutex RWMutexType;
        FdManager();

        FdCtx::ptr Get(int fd, bool auto_create = false);
        void Del(int fd);

    private:
        RWMutexType mMutex;
        std::vector<FdCtx::ptr> mDatas;
    };

    typedef Singleton<FdManager> FdMgr;
} // namespace sylar

#endif // !__FDMANAGER_H__