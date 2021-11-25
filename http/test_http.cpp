#include "http.h"
#include "../sock/sock.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

int main(int argc,char* argv[])
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n",argv[0]);
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi( argv[2] );
    
    int listenfd = create_socket(ip,port);

    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof( client_address );

    int fd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
    if( fd < 0 )
    {
        printf( "errno is: %d\n", errno );
    }
    else {
        HTTP http(fd);
        http.process();
    }
    
    close( listenfd );

    return 0;
}
