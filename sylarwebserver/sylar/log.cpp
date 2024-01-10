#include "log.h"
#include <map>
#include <iostream>
#include "config.h"
namespace sylar
{
    // level
    const char *LogLevel::ToString(LogLevel::Level level)
    {
        switch (level)
        {
#define XX(name)         \
    case LogLevel::name: \
        return #name;    \
        break;

            XX(DEBUG);
            XX(INFO);
            XX(WARN);
            XX(ERROR);
            XX(FATAL);
#undef XX
        default:
            return "UNKONW";
        }
        return "UNKONW";
    }
    LogLevel::Level LogLevel::FromString(const std::string &str)
    {
#define XX(level, v)            \
    if (str == #v)              \
    {                           \
        return LogLevel::level; \
    }
        XX(DEBUG, debug);
        XX(INFO, info);
        XX(WARN, warn);
        XX(ERROR, error);
        XX(FATAL, fatal);

        XX(DEBUG, DEBUG);
        XX(INFO, INFO);
        XX(WARN, WARN);
        XX(ERROR, ERROR);
        XX(FATAL, FATAL);
        return UNKONW;
#undef XX
    }

    LogEventWrap::LogEventWrap(LogEvent::ptr e) : mEvent(e) {}
    // 真正的接口
    LogEventWrap::~LogEventWrap()
    {
        mEvent->GetLogger()->Log(mEvent->GetLevel(), mEvent);
    }
    std::stringstream &LogEventWrap::GetSS()
    {
        return mEvent->GetSS();
    }
    // al是一个指针,vastart(al,fmt)指的是给al赋值，值为第一个可变参数的地址
    void LogEvent::Format(const char *fmt, ...)
    {
        va_list al;
        va_start(al, fmt);
        Format(fmt, al);
        va_end(al);
    }
    // vasprintf中 1. 承接字符串的地址。   2. 被替换的字符串，可以被format格式化的替换%x等。 3.可变参数的地址。
    void LogEvent::Format(const char *fmt, va_list al)
    {
        char *buf = nullptr;
        int len = vasprintf(&buf, fmt, al);
        if (len != -1)
        {
            mSs << std::string(buf, len);
            free(buf);
        }
    }

    void LogAppender::SetFormatter(LogFormatter::ptr val)
    {
        MutexType::MutexLock Lock(mMutex);
        mFormatter = val;
        if (mFormatter)
        {
            mHasFormatter = true;
        }
        else
        {
            mHasFormatter = false;
        }
    }
    LogFormatter::ptr LogAppender::GetFormatter()
    {
        MutexType::MutexLock Lock(mMutex);
        return mFormatter;
    }

    // 日志文本
    class MessageFormatItem : public LogFormatter::FormatItem
    {
    public:
        MessageFormatItem(const std::string &str = "") {}
        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetContent();
        }
    };
    // 日志等级
    class LevelFormatItem : public LogFormatter::FormatItem
    {
    public:
        LevelFormatItem(const std::string &str = "") {}
        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << LogLevel::ToString(level);
        }
    };
    // 到现在的毫秒数
    class ElapseFormatItem : public LogFormatter::FormatItem
    {
    public:
        ElapseFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetElapse();
        }
    };
    // 日志名
    class NameFormatItem : public LogFormatter::FormatItem
    {
    public:
        NameFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetLogger()->GetName();
        }
    };
    // 线程名
    class ThreadNameFormatItem : public LogFormatter::FormatItem
    {
    public:
        ThreadNameFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetThreadName();
        }
    };
    // 线程号
    class ThreadIdFormatItem : public LogFormatter::FormatItem
    {
    public:
        ThreadIdFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetTheadId();
        }
    };
    // 协程号
    class FiberIdFormatItem : public LogFormatter::FormatItem
    {
    public:
        FiberIdFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetFiberId();
        }
    };
    // 时间戳
    class DateTimeFormatItem : public LogFormatter::FormatItem
    {
    public:
        DateTimeFormatItem(const std::string &format = "%Y-%m-%d %H:%M:%S") : mFormat(format)
        {
            if (mFormat.empty())
            {
                mFormat = "%Y-%m-%d %H:%M:%S";
            }
        }
        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            struct tm tm;
            time_t time = event->GetTime();
            localtime_r(&time, &tm);
            char buf[64];
            strftime(buf, sizeof(buf), mFormat.c_str(), &tm);
            os << buf;
        }

    private:
        std::string mFormat;
    };
    // 文件名
    class FileNameFormatItem : public LogFormatter::FormatItem
    {
    public:
        FileNameFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetFile();
        }
    };
    // 行号
    class LineFormatItem : public LogFormatter::FormatItem
    {
    public:
        LineFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << event->GetLine();
        }
    };
    // 字符串
    class StringFormatItem : public LogFormatter::FormatItem
    {
    public:
        StringFormatItem(const std::string &str)
            : mString(str) {}
        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << mString;
        }

    private:
        std::string mString;
    };
    // 换行
    class NewLineFormatItem : public LogFormatter::FormatItem
    {
    public:
        NewLineFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << std::endl;
        }
    };
    // tab
    class TabFormatItem : public LogFormatter::FormatItem
    {
    public:
        TabFormatItem(const std::string &str = "") {}

        void Format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override
        {
            os << "\t";
        }
    };

    // LogEvent
    LogEvent::LogEvent(Logger::ptr logger, LogLevel::Level level, const char *file, int32_t line, uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint32_t time, const std::string &threadName)
        : mFile(file), mLine(line), mElapse(elapse), mThreadId(threadId), mFiberId(fiberId), mTime(time), mLogger(logger), mLevel(level), mThreadName(threadName)
    {
    }

    // logger
    // %m -- 消息体
    // %p -- level
    // %r -- 启动后时间
    // %c -- 日志名称
    // %t -- 线程id
    // %n -- 回车换行
    // %d -- 时间
    // %f -- 文件名
    // %l -- 行号
    // %T -- Tab
    Logger::Logger(const std::string &name)
        : mName(name), mLevel(LogLevel::DEBUG)
    {
        mFormatter.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T%F%T[%p]%T[%c]%T<%f:%l>%T%m%n"));
    }
    std::string Logger::ToYamlString()
    {
        MutexType::MutexLock Lock(mMutex);
        YAML::Node node;
        node["name"] = mName;
        if (mLevel != LogLevel::UNKONW)
        {
            node["level"] = LogLevel::ToString(mLevel);
        }

        if (mFormatter)
        {
            node["formatter"] = mFormatter->GetPattern();
        }
        for (auto &i : mAppenders)
        {
            node["appenders"].push_back(YAML::Load(i->ToYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    void Logger::SetFormatter(LogFormatter::ptr val)
    {
        MutexType::MutexLock Lock(mMutex);
        mFormatter = val;

        for (auto &i : mAppenders)
        {
            MutexType::MutexLock LL(i->mMutex);
            if (!i->mHasFormatter)
            {
                i->mFormatter = mFormatter;
            }
        }
    }
    void Logger::SetFormatter(const std::string &val)
    {
        sylar::LogFormatter::ptr new_val(new sylar::LogFormatter(val));
        if (new_val->IsError())
        {
            std::cout << "Logger::SetFormatter name=" << mName << " value=" << val << " invalid formatter" << std::endl;
            return;
        }
        SetFormatter(new_val);
    }
    LogFormatter::ptr Logger::GetFormatter()
    {
        MutexType::MutexLock Lock(mMutex);
        return mFormatter;
    }

    void Logger::AddAppender(LogAppender::ptr appender)
    {
        MutexType::MutexLock Lock(mMutex);

        if (!appender->GetFormatter())
        {
            MutexType::MutexLock Lock(appender->mMutex);

            appender->mFormatter = mFormatter; // 空，把Logger的赋值给他
        }
        mAppenders.push_back(appender);
    }
    void Logger::DelAppender(LogAppender::ptr appender)
    {
        MutexType::MutexLock Lock(mMutex);

        for (auto it = mAppenders.begin(); it != mAppenders.end(); it++)
        {
            if (*it == appender)
            {
                mAppenders.erase(it);
                break;
            }
        }
    }
    void Logger::ClearAppenders()
    {
        MutexType::MutexLock Lock(mMutex);

        mAppenders.clear();
    }
    void Logger::Log(LogLevel::Level level, LogEvent::ptr event)
    {
        if (level >= mLevel)
        {
            auto self = shared_from_this();
            MutexType::MutexLock Lock(mMutex);

            if (!mAppenders.empty())
            {
                for (auto &i : mAppenders)
                {
                    i->Log(self, level, event); // 输出到每个appender
                }
            }
            else if (mRoot)
            {
                mRoot->Log(level, event);
            }
        }
    }

    void Logger::DeBug(LogEvent::ptr event)
    {
        Log(LogLevel::DEBUG, event);
    }
    void Logger::InFo(LogEvent::ptr event)
    {
        Log(LogLevel::INFO, event);
    }
    void Logger::Warn(LogEvent::ptr event)
    {
        Log(LogLevel::WARN, event);
    }
    void Logger::Error(LogEvent::ptr event)
    {
        Log(LogLevel::ERROR, event);
    }
    void Logger::Fatal(LogEvent::ptr event)
    {
        Log(LogLevel::FATAL, event);
    }
    // appender

    // 标准输出日志---------------------
    void StdoutLogAppender::Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if (level >= mLevel)
        {
            MutexType::MutexLock Lock(mMutex);
            std::cout << mFormatter->Format(logger, level, event);
        }
    }
    std::string StdoutLogAppender::ToYamlString()
    {
        MutexType::MutexLock Lock(mMutex);
        YAML::Node node;
        node["type"] = "StdoutLogAppender";
        if (mLevel != LogLevel::UNKONW)
        {
            node["level"] = LogLevel::ToString(mLevel);
        }
        if (mFormatter && mHasFormatter)
        {
            node["formatter"] = mFormatter->GetPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    // 文件输出日志---------------------
    bool FileLogAppender::reopen()
    {
        if (mFileStream)
        {
            mFileStream.close();
        }
        mFileStream.open(mFileName);
        return !mFileStream;
    }
    FileLogAppender::FileLogAppender(const std::string &filename) : mFileName(filename)
    {
        MutexType::MutexLock Lock(mMutex);
        reopen();
    }
    void FileLogAppender::Log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        if (level >= mLevel)
        {
            uint64_t now = time(0);
            if (now != mLastTime)
            {
                reopen();
                mLastTime = now;
            }
            MutexType::MutexLock Lock(mMutex);
            if (!(mFileStream << mFormatter->Format(logger, level, event)))
            {
                std::cout << "error" << std::endl;
            }
        }
    }

    std::string FileLogAppender::ToYamlString()
    {
        MutexType::MutexLock Lock(mMutex);
        YAML::Node node;
        node["type"] = "FileLogAppender";
        node["file"] = mFileName;
        if (mLevel != LogLevel::UNKONW)
        {
            node["level"] = LogLevel::ToString(mLevel);
        }
        if (mFormatter && mHasFormatter)
        {
            node["formatter"] = mFormatter->GetPattern();
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }

    // formatter
    LogFormatter::LogFormatter(const std::string &pattern)
        : mPattern(pattern)
    {
        init();
    }
    std::string LogFormatter::Format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
    {
        std::stringstream ss;
        for (auto &i : mItems)
        {
            i->Format(ss, logger, level, event);
        }
        return ss.str();
    }

    //%xxx , %xxx{xxx} , %%
    void LogFormatter::init()
    {
        std::vector<std::tuple<std::string, std::string, int>> vec;
        std::string nstr;

        for (size_t i = 0; i < mPattern.size(); ++i)
        {
            // 寻找%
            if (mPattern[i] != '%')
            {
                nstr.append(1, mPattern[i]);
                continue;
            }
            // 接下来mPattern[i] = '%'
            // 排除%%情况
            if ((i + 1) < mPattern.size())
            {
                if (mPattern[i + 1] == '%')
                {
                    nstr.append(1, '%');
                    continue;
                }
            }

            size_t n = i + 1;
            int fmtStatus = 0;   // 标记现在的解析状态，没遇到{之前为0，遇到{之后没遇到}之前为1，遇到}之后为2
            size_t fmtBegin = 0; // 标记{位置

            std::string str; //%xxx 中的xxx
            std::string fmt; //%xxx{xxx} 中的第二部分xxx

            // %xxx与%xxx{xxx}

            while (n < mPattern.size())
            {
                // %d%t%n ,第一个if的作用在于匹配到i=d，n=%时（未进入{}内，不是字母，也不是左右大括号，一般是%或非格式字符，就会把，d放入str中。
                if (!fmtStatus && !isalpha(mPattern[n]) && mPattern[n] != '{' && mPattern[n] != '}')
                {
                    str = mPattern.substr(i + 1, n - i - 1);
                    break;
                }
                if (fmtStatus == 0)
                {
                    if (mPattern[n] == '{')
                    {
                        str = mPattern.substr(i + 1, n - i - 1);

                        fmtStatus = 1;
                        fmtBegin = n;
                        ++n;
                        continue;
                    }
                }
                if (fmtStatus == 1)
                {
                    if (mPattern[n] == '}')
                    {
                        fmt = mPattern.substr(fmtBegin + 1, n - fmtBegin - 1);
                        fmtStatus = 0;
                        n++;
                        break;
                    }
                }
                ++n;
                if (n == mPattern.size())
                {
                    if (str.empty())
                        str = mPattern.substr(i + 1);
                }
            }
            // 没有{}或者已经截取到fmt中
            if (fmtStatus == 0)
            {
                if (!nstr.empty())
                {
                    vec.push_back(std::make_tuple(nstr, "", 0));
                    nstr.clear();
                }

                vec.push_back(std::make_tuple(str, fmt, 1));
                i = n - 1;
            }
            else if (fmtStatus == 1)
            {
                std::cout << "pattern parse error: " << mPattern << " - " << mPattern.substr(i) << std::endl; // substr(i) --- 裁剪从i到尾的字符串
                mError = true;
                vec.push_back(std::make_tuple("<<pattern_error", fmt, 0));
            }

        } // for
        // 没有百分号的情况
        if (!nstr.empty())
        {
            vec.push_back(std::make_tuple(nstr, "", 0));
        }
        // map中存储的是string，funtion；
        //  = {m,function包装的lamda表达式而lamda表达式被define所批量赋予}
        // fmt即上文搞到的，这里为了统一格式，一些不需要fmt的item类也有空的传入，通常只有datetimeitem类有
        static std::map<std::string, std::function<FormatItem::ptr(const std::string &str)>> sFormatItems = {
#define XX(str, C)                                                               \
    {                                                                            \
        #str, [](const std::string &fmt) { return FormatItem::ptr(new C(fmt)); } \
    }

            XX(m, MessageFormatItem),
            XX(p, LevelFormatItem),
            XX(r, ElapseFormatItem),
            XX(c, NameFormatItem),
            XX(t, ThreadIdFormatItem),
            XX(n, NewLineFormatItem),
            XX(d, DateTimeFormatItem),
            XX(f, FileNameFormatItem),
            XX(l, LineFormatItem),
            XX(T, TabFormatItem),
            XX(F, FiberIdFormatItem),
            XX(N, ThreadNameFormatItem),
#undef XX
        };

        for (auto &i : vec)
        {
            if (std::get<2>(i) == 0) // 无百分号
            {
                mItems.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
            }
            else
            {
                auto it = sFormatItems.find(std::get<0>(i));
                if (it == sFormatItems.end())
                {
                    std::cout << "错误2"
                              << "<<error_format %" + std::get<0>(i) + ">>" << std::endl;
                    mItems.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
                    mError = true;
                }
                else
                {
                    mItems.push_back(it->second(std::get<1>(i)));
                }
            }
            // std::cout << "(" << std::get<0>(i) << ") - (" << std::get<1>(i) << ") - (" << std::get<2>(i) << ")" << std::endl;
        }

        // %m -- 消息体
        // %p -- level
        // %r -- 启动后时间
        // %c -- 日志名称
        // %t -- 线程id
        // %n -- 回车换行
        // %d -- 时间
        // %f -- 文件名
        // %l -- 行号
        // %T -- Tab
    }
    LoggerManager::LoggerManager()
    {
        mRoot.reset(new Logger);
        mRoot->AddAppender(LogAppender::ptr(new StdoutLogAppender));
        mLoggers[mRoot->mName] = mRoot;

        init();
    }
    Logger::ptr LoggerManager::GetLogger(const std::string &name)
    {
        MutexType::MutexLock Lock(mMutex);
        auto it = mLoggers.find(name);
        if (it != mLoggers.end())
        {
            return it->second;
        }
        Logger::ptr logger(new Logger(name));
        logger->mRoot = mRoot;
        mLoggers[name] = logger;
        return logger;
    }

    struct LogAppenderDefine
    {
        int type = 0; // 1 file ,2 stdout
        LogLevel::Level level = LogLevel::UNKONW;
        std::string formatter;
        std::string file;

        bool operator==(const LogAppenderDefine &oth) const
        {
            return type == oth.type && level == oth.level && formatter == oth.formatter && file == oth.file;
        }
    };
    struct LogDefine
    {
        std::string name;
        LogLevel::Level level = LogLevel::UNKONW;
        std::string formatter;
        std::vector<LogAppenderDefine> appenders;
        bool operator==(const LogDefine &oth) const
        {
            return name == oth.name && level == oth.level && formatter == oth.formatter && appenders == oth.appenders;
        }
        bool operator<(const LogDefine &oth) const
        {
            return name < oth.name;
        }
    };
    // 自定义logdefine类偏特化
    template <>
    class LexicalCast<std::string, LogDefine>
    {
    public:
        LogDefine operator()(const std::string &v)
        {
            YAML::Node n = YAML::Load(v);
            LogDefine ld;
            if (!n["name"].IsDefined())
            { // name 为空,抛出异常
                std::cout << "log config name is null" << n << std::endl;
                throw std::logic_error("log config is null");
            }
            ld.name = n["name"].as<std::string>();
            if (n["level"].IsDefined())
            {
                ld.level = LogLevel::FromString(n["level"].as<std::string>());
            }
            else
            {
                ld.level = LogLevel::FromString("");
                std::cout << "log config error: level is null " << std::endl;
            }
            if (n["formatter"].IsDefined())
            {
                ld.formatter = n["formatter"].as<std::string>();
            }

            // appenders
            if (n["appenders"].IsDefined())
            {
                for (size_t x = 0; x < n["appenders"].size(); x++)
                {
                    auto a = n["appenders"][x];
                    if (!a["type"].IsDefined())
                    {
                        std::cout << "log config error: appender type is null" << a << std::endl;
                        continue;
                    }
                    std::string type = a["type"].as<std::string>();
                    LogAppenderDefine lad;
                    if (type == "FileLogAppender")
                    {
                        lad.type = 1;
                        if (!a["file"].IsDefined())
                        {
                            std::cout << "log config error: fileappender file is null" << a << std::endl;
                            continue;
                        }
                        lad.file = a["file"].as<std::string>();
                        if (a["formatter"].IsDefined())
                        {
                            lad.formatter = a["formatter"].as<std::string>();
                        }
                    }
                    else if (type == "StdoutLogAppender")
                    {
                        lad.type = 2;
                        if (a["formatter"].IsDefined())
                        {
                            lad.formatter = a["formatter"].as<std::string>();
                        }
                    }
                    else
                    {
                        std::cout << "log config error: appender type is invalid, " << a << std::endl;
                        continue;
                    }
                    ld.appenders.push_back(lad);
                }
            }
            return ld;
        }
    };
    template <>
    class LexicalCast<LogDefine, std::string>
    {
    public:
        std::string operator()(const LogDefine &i)
        {
            YAML::Node n;
            n["name"] = i.name;
            if (i.level != LogLevel::UNKONW)
            {
                n["level"] = LogLevel::ToString(i.level);
            }
            if (!i.formatter.empty())
            {
                n["formatter"] = i.formatter;
            }
            for (auto &a : i.appenders)
            {
                YAML::Node na;
                if (a.type == 1)
                {
                    na["type"] = "FileLogAppender";
                    na["file"] = a.file;
                }
                else if (a.type == 2)
                {
                    na["type"] = "StdoutLogAppender";
                }
                if (a.level != LogLevel::UNKONW)
                {
                    na["level"] = LogLevel::ToString(a.level);
                }
                if (!a.formatter.empty())
                {
                    n["formatter"] = a.formatter;
                }
                n["appenders"].push_back(na);
            }
            std::stringstream ss;
            ss << n;
            return ss.str();
        }
    };

    sylar::ConfigVar<std::set<LogDefine>>::ptr g_log_defines = sylar::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

    struct LogIniter
    {
        LogIniter()
        {
            g_log_defines->AddListener([](const std::set<LogDefine> &oldValue, const std::set<LogDefine> &newValue)
                                       {
                SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "on_logger_conf_changed";
                for (auto &i : newValue)
                {
                    auto it = oldValue.find(i);
                    sylar::Logger::ptr logger;
                    if (it == oldValue.end()) // 老的没有，说明新增
                    {
                        // 新增
                        logger = SYLAR_LOG_NAME(i.name);   //name
                    }
                    else //新老都有，比较一下是否相等
                    {
                        if(!(i == *it))//两者不相等，修改
                        {
                            //修改
                            logger = SYLAR_LOG_NAME(i.name);
                        }
                    }
                    logger->SetLevel(i.level);                              //level
                        if(!i.formatter.empty())                                //formatter
                        {
                            // i的配置中formatter不为空，说明要手动修改添加,为空则直接用默认的正确的formatter
                            logger->SetFormatter(i.formatter);//set如果传入的是string会new一个，构造函数会有解析，传入的是ptr的话就直接赋值
                        }
                        logger->ClearAppenders();                               //appenders
                        for(auto & a: i.appenders)
                        {
                            sylar::LogAppender::ptr ap;
                            if(a.type == 1)
                            {
                                // fileappender
                                ap.reset(new sylar::FileLogAppender(a.file));
                            }
                            else if(a.type == 2)
                            {
                                // stdout
                                ap.reset(new sylar::StdoutLogAppender());
                            }
                            ap->SetLevel(a.level);
                            if(!a.formatter.empty())
                            {
                                LogFormatter::ptr fmt(new LogFormatter(a.formatter));
                                if(!fmt->IsError())
                                {
                                    ap->SetFormatter(fmt);
                                }
                                else
                                {
                                    std::cout << "log.name: " << i.name << " appender type=" << a.type << " formatter=" << a.formatter << " is invalid" << std::endl;
                                }
                                
                            }
                            logger->AddAppender(ap);
                        }
                }

                // 删除 ,新的无老的有
                for(auto & i: oldValue)
                {
                    auto it = newValue.find(i);
                    if(it == newValue.end())
                    {
                        // 删除
                        auto logger = SYLAR_LOG_NAME(i.name);
                        logger->SetLevel((LogLevel::Level)100);
                        logger->ClearAppenders();
                    }
                } });
        }
    };

    // 静态全局结构体会在main之前构造，利用其构造函数设置监听
    static LogIniter __log_init;
    std::string LoggerManager::ToYamlString()
    {
        MutexType::MutexLock Lock(mMutex);
        YAML::Node node;
        for (auto &i : mLoggers)
        {
            node.push_back(YAML::Load(i.second->ToYamlString()));
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
    void LoggerManager::init()
    {
    }

} // namespace sylar
