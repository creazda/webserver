#include "../sylar/hook.h"
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include <netinet/in.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <arpa/inet.h>

sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();

void test_sleep()
{
    sylar::IOManager iom(1);
    iom.schedule([]()
                 {
        SYLAR_LOG_INFO(g_logger) << "sleep 2 begin";
        sleep(2);
        SYLAR_LOG_INFO(g_logger) << "sleep 2 end"; });

    iom.schedule([]()
                 {
        SYLAR_LOG_INFO(g_logger) << "sleep 3 begin";
        sleep(3);
        SYLAR_LOG_INFO(g_logger) << "sleep 3 end"; });
    SYLAR_LOG_INFO(g_logger) << "test_sleep";
}
void test_sock()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "39.156.66.10", &addr.sin_addr.s_addr);

    int rt = connect(sock, (const sockaddr *)&addr, sizeof(addr));
    SYLAR_LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;
    if (rt)
    {
        SYLAR_LOG_INFO(g_logger) << "connect error ";
        return;
    }
    const char data[] = "GET / HTTP/1.0\r\n\r\n";
    rt = send(sock, data, sizeof(data), 0);
    SYLAR_LOG_INFO(g_logger) << "send rt = " << rt << " errno=" << errno;

    if (rt <= 0)
    {
        return;
    }
    std::cout << "1" << std::endl;
    std::string buff;
    buff.resize(4096);
    std::cout << "2" << std::endl;

    rt = recv(sock, &buff[0], buff.size(), 0);
    std::cout << "3" << std::endl;

    SYLAR_LOG_INFO(g_logger) << "recv rt = " << rt << " errno = " << errno;
    if (rt < 0)
    {
        return;
    }
    buff.resize(rt);
    SYLAR_LOG_INFO(g_logger) << buff;
}

int main(int argc, char **argv)
{
    // test_sleep();
    // test_sock();
    sylar::IOManager iom;
    iom.schedule(test_sock);

    return 0;
}