#include "sylar/sylar.h"
#include "sylar/iomanager.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "arpa/inet.h"
#include "unistd.h"
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

int sock = 0;
void test_fiber()
{
    SYLAR_LOG_INFO(g_logger) << "test_fiber sock=" << sock;

    sock = socket(AF_INET, SOCK_STREAM, 0); // 获取套接字文件描述符
    SYLAR_LOG_INFO(g_logger) << "test_fiber sock=" << sock;

    fcntl(sock, F_SETFL, O_NONBLOCK); // 设置为非阻塞

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "39.156.66.10", &addr.sin_addr.s_addr);

    if (!connect(sock, (const sockaddr *)&addr, sizeof(addr)))
    { // 成功啥也不做
        SYLAR_LOG_DEBUG(g_logger) << "connect success";
    }
    else if (errno == EINPROGRESS)
    {
        SYLAR_LOG_INFO(g_logger) << "add event error=" << errno << " " << strerror(errno);
        sylar::IOManager::GetThis()->AddEvent(sock, sylar::IOManager::READ, []()
                                              { SYLAR_LOG_INFO(g_logger) << "connected"; });
        sylar::IOManager::GetThis()->AddEvent(sock, sylar::IOManager::WRITE, []()
                                              { SYLAR_LOG_INFO(g_logger) << "connected"; });
    }
    else
    {
        SYLAR_LOG_INFO(g_logger) << "else " << errno << " " << strerror(errno);
    }
}
void test1()
{
    sylar::IOManager iom(2, false, "socket调度器");
    iom.schedule(&test_fiber);
}
sylar::Timer::ptr s_timer;
void test_timer()
{
    sylar::IOManager iom(2);
    SYLAR_LOG_DEBUG(g_logger) << "管理器构造完毕";
    s_timer = iom.AddTimer(
        1000, []()
        { 
            static int i = 0;
            SYLAR_LOG_INFO(g_logger) << "hello TImer i====:" << i; 
            
            if(++i == 3)
            {   
                //s_timer->Cancel();
                s_timer->Reset(2000,true);
            } },
        true);
    SYLAR_LOG_DEBUG(g_logger) << "addtimer 成功";
    iom.schedule(&test_fiber);
}
int main(int argc, char const *argv[])
{
    // test1();
    test_timer();
    return 0;
}
