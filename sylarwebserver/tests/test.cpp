#include <iostream>
#include "../sylar/log.h"
#include "../sylar/util.h"

int main(int argc, char **argv)
{
    sylar::Logger::ptr logger(new sylar::Logger);
    logger->AddAppender(sylar::LogAppender::ptr(new sylar::StdoutLogAppender));
    sylar::FileLogAppender::ptr file_appender(new sylar::FileLogAppender("./log.txt"));
    sylar::LogFormatter::ptr fmt(new sylar::LogFormatter("%d%T%p%T%m%n"));
    file_appender->SetFormatter(fmt);
    logger->AddAppender(file_appender);
    file_appender->SetLevel(sylar::LogLevel::ERROR);
    // sylar::LogEvent::ptr event(new sylar::LogEvent(logger, sylar::LogLevel::DEBUG, __FILE__, __LINE__, 0, sylar::UtilGetThreadId(), sylar::UtilGetFiberId(), time(0)));
    // logger->Log(sylar::LogLevel::DEBUG, event);
    std::cout << "hello sylar log" << std::endl;
    SYLAR_LOG_INFO(logger) << "test macro";
    SYLAR_LOG_ERROR(logger) << "test macro error";
    SYLAR_LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");

    auto i = sylar::LoggerMgr::GetInstance()->GetLogger("xx");
    SYLAR_LOG_INFO(i) << "xxx";

    return 0;
}
