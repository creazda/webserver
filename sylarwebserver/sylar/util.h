#ifndef __SYLAR_UTIL_H__
#define __SYLAR_UTIL_H__

#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <iostream>
namespace sylar
{
    pid_t UtilGetThreadId();
    uint32_t UtilGetFiberId();

    void Backtrace(std::vector<std::string> &bt, int size = 64, int skip = 1);
    std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");

    // 时间
    uint64_t GetCurrentMS();
    uint64_t GetCurrentUS();
}

#endif // __SYLAR_UTIL_H__
