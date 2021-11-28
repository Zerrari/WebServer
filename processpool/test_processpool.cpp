#include "processpool.h"
#include <iostream>
#include <sys/event.h>
#include "../sock/sock.h"
#include "../http/http.h"

int main(int argc, char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int listenfd = create_socket(ip,port);

    printf("listenfd: %d\n",listenfd);

    int kq = kqueue();

    processpool<HTTP>* pool = processpool<HTTP>::create(listenfd, kq, 1);

    pool->run();

    return 0;
} 
