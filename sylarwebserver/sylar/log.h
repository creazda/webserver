#ifndef __SYLAR_LOG_H__
#define __SYLAR_LOG_H__

#include <iostream>
#include <string>
#include <stdint.h>
#include <memory>
#include <list>
#include <sstream>
#include <fstream>
#include <vector>
#include <functional>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include "util.h"
#include <map>
#include "singleton.h"
#include "sylar/mutex.h"
#include "thread.h"

#define SYLAR_LOG_LEVEL(logger, level)                                                                                           \
    if (logger->GetLevel() <= level)                                                                                             \
    sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, 0, sylar::UtilGetThreadId(), \
                                                                 sylar::UtilGetFiberId(), time(0), sylar::Thread::S_GetName()))) \
        .GetSS()
#define SYLAR_LOG_DEBUG(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::DEBUG)
#define SYLAR_LOG_INFO(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::INFO)
#define SYLAR_LOG_WARN(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::WARN)
#define SYLAR_LOG_ERROR(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::ERROR)
#define SYLAR_LOG_FATAL(logger) SYLAR_LOG_LEVEL(logger, sylar::LogLevel::FATAL)

#define SYLAR_LOG_FMT_LEVEL(logger, level, fmt, ...)                                                                             \
    if (logger->GetLevel() <= level)                                                                                             \
    sylar::LogEventWrap(sylar::LogEvent::ptr(new sylar::LogEvent(logger, level, __FILE__, __LINE__, 0, sylar::UtilGetThreadId(), \
                                                                 sylar::UtilGetFiberId(), time(0), sylar::Thread::S_GetName()))) \
        .GetEvent()                                                                                                              \
        ->Format(fmt, ##__VA_ARGS__)

#define SYLAR_LOG_FMT_DEBUG(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::DEBUG, ##__VA_ARGS__)
#define SYLAR_LOG_FMT_INFO(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::INFO, ##__VA_ARGS__)
#define SYLAR_LOG_FMT_WARN(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::WARN, ##__VA_ARGS__)
#define SYLAR_LOG_FMT_ERROR(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::ERROR, ##__VA_ARGS__)
#define SYLAR_LOG_FMT_FATAL(logger, fmt, ...) SYLAR_LOG_FMT_LEVEL(logger, sylar::LogLevel::FATAL, ##__VA_ARGS__)

#define SYLAR_LOG_ROOT() sylar::LoggerMgr::GetInstance()->GetRoot()
#define SYLAR_LOG_NAME(name) sylar::LoggerMgr::GetInstance()->GetLogger(name)

namespace sylar
{
    class Logger;
    class LoggerManager;
    // 日志级别
    class LogLevel
    {
    public:
        enum Level
        {
            UNKONW = 0,
            DEBUG = 1,
            INFO = 2,
            WARN = 3,
            ERROR = 4,
            FATAL = 5
        };
        static const char *ToString(LogLevel::Level level);
        static LogLevel::Level FromString(const std::string &str);
    };
    // 日志事件
    class LogEvent
    {
    public:
        typedef std::shared_ptr<LogEvent> ptr;
        LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint32_t time, const std::string &threadName);

        const char *GetFile() const { return mFile; }
        int32_t GetLine() const { return mLine; }
        uint32_t GetElapse() const { return mElapse; }
        uint32_t GetTheadId() const { return mThreadId; }
        uint32_t GetFiberId() const { return mFiberId; }
        uint32_t GetTime() const { return mTime; }
        const std::string &GetThreadName() const { return mThreadName; }
        std::string GetContent() const { return mSs.str(); }
        std::stringstream &GetSS() { return mSs; }
        std::shared_ptr<Logger> GetLogger() const { return mLogger; }
        LogLevel::Level GetLevel() const { return mLevel; }

        void Format(const char *fmt, ...);
        void Format(const char *fmt, va_list al);

    private:
        const char *mFile = nullptr; // 文件名
        int32_t mLine = 0;           // 行号
        uint32_t mElapse = 0;        // 程序启动开始到现在的毫秒数
        uint32_t mThreadId = 0;      // 线程id
        uint32_t mFiberId = 0;       // 协程Id
        uint32_t mTime = 0;          // 时间戳
        std::stringstream mSs;       // 日志文本
        std::shared_ptr<Logger> mLogger;
        LogLevel::Level mLevel;
        std::string mThreadName; // 线程名
    };
    class LogEventWrap
    {
    public:
        LogEventWrap(LogEvent::ptr e);
        ~LogEventWrap();
        std::stringstream &GetSS();
        LogEvent::ptr GetEvent() const { return mEvent; }

    private:
        LogEvent::ptr mEvent;
    };

    // 日志格式器
    class LogFormatter
    {
    public:
        typedef std::shared_ptr<LogFormatter> ptr;
        LogFormatter(const std::string &pattern);
        std::string Format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

    public:
        class FormatItem
        {
        public:
            typedef std::shared_ptr<FormatItem> ptr;
            virtual ~FormatItem() {}
            virtual void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
        };

        void init();
        bool IsError() const { return mError; }
        const std::string GetPattern() const { return mPattern; }

    private:
        std::string mPattern;
        std::vector<FormatItem::ptr> mItems;
        bool mError = false;
    };
    // 日志输出地
    class LogAppender
    {
        friend class Logger;

    public:
        typedef std::shared_ptr<LogAppender> ptr;
        typedef Spinlock MutexType;
        virtual ~LogAppender() {}

        virtual void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;
        virtual std::string ToYamlString() = 0;
        void SetFormatter(LogFormatter::ptr val);
        LogFormatter::ptr GetFormatter();
        LogLevel::Level GetLevel() const { return mLevel; }
        void SetLevel(LogLevel::Level val) { mLevel = val; }

    protected:
        LogLevel::Level mLevel = LogLevel::DEBUG;
        bool mHasFormatter = false;
        MutexType mMutex;
        LogFormatter::ptr mFormatter;
    };

    // 日志器
    class Logger : public std::enable_shared_from_this<Logger>
    {
        friend class LoggerManager;

    public:
        typedef std::shared_ptr<Logger> ptr;
        typedef Spinlock MutexType;
        Logger(const std::string &name = "root");
        void Log(LogLevel::Level level, LogEvent::ptr event);

        void DeBug(LogEvent::ptr event);
        void InFo(LogEvent::ptr event);
        void Warn(LogEvent::ptr event);
        void Error(LogEvent::ptr event);
        void Fatal(LogEvent::ptr event);

        void AddAppender(LogAppender::ptr appender);
        void DelAppender(LogAppender::ptr appender);
        void ClearAppenders();
        LogLevel::Level GetLevel() const { return mLevel; };
        void SetLevel(LogLevel::Level val) { mLevel = val; };

        const std::string &GetName() { return mName; };
        void SetFormatter(LogFormatter::ptr val);
        void SetFormatter(const std::string &val);
        LogFormatter::ptr GetFormatter();

        std::string ToYamlString();

    private:
        std::string mName;                      // 日志名称
        LogLevel::Level mLevel;                 // 日志级别
        std::list<LogAppender::ptr> mAppenders; // appender集合
        LogFormatter::ptr mFormatter;
        Logger::ptr mRoot;
        MutexType mMutex;
    };

    // 输出到控制台
    class StdoutLogAppender : public LogAppender
    {
    public:
        typedef std::shared_ptr<StdoutLogAppender> ptr;
        void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
        std::string ToYamlString() override;

    private:
    };
    // 输出到文件
    class FileLogAppender : public LogAppender
    {
    public:
        typedef std::shared_ptr<FileLogAppender> ptr;
        FileLogAppender(const std::string &filename);
        void Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
        std::string ToYamlString() override;

        // 重新打开文件，文件返回成功返回true
        bool reopen();

    private:
        std::string mFileName;
        std::ofstream mFileStream;
        uint64_t mLastTime = 0;
    };

    // 日志管理器
    class LoggerManager
    {
    public:
        typedef Spinlock MutexType;
        LoggerManager();
        Logger::ptr GetLogger(const std::string &name);
        void init();
        Logger::ptr GetRoot() const { return mRoot; };
        std::string ToYamlString();

    private:
        std::map<std::string, Logger::ptr> mLoggers;
        Logger::ptr mRoot;
        MutexType mMutex;
    };

    typedef sylar::Singleton<LoggerManager> LoggerMgr;
} // namespace sylar

#endif // !__SYLAR_LOG_H__
