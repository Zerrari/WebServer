#include "sock.h"

#include <cstring>
#include <cassert>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

int create_socket(const char* ip,int port)
{
    
    // 创建监听套接字的地址结构
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;

    // 将IP地址从表达式转换成数值形式
    // 表达式：IP地址的字符串形式
    // 数值形式：IP地址的二进制形式
    inet_pton(AF_INET, ip, &address.sin_addr);

    // 将端口号从主机字节序转换成网络字节序
    address.sin_port = htons(port);

    // 创建套接字描述符
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);

    // SO_REUSEADDR允许端口被重复使用，使一个端口可以在TIME_WAIT期间被绑定使用
    int flag = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    
    // 将套接字描述符绑定到地址
    int ret = bind(listenfd, ( struct sockaddr* )&address, sizeof( address ));
    assert( ret != -1 );
    
    // 创建监听队列存放待处理的连接，backlog表示半连接队列的连接数量
    ret = listen(listenfd, 5);
    assert( ret != -1 );

    return listenfd;
}
