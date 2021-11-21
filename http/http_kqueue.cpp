#include "http.h"
#include "../sock/sock.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

#define USER_LIMIT 5

// 设置文件描述符为非阻塞
int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }


    const char* ip = argv[1];
    int port = atoi( argv[2] );

    int listenfd = create_socket(ip,port);

    printf("listenfd: %d\n",listenfd);

    // 储存客户端数据的数组
    int user_count = 0;

    // create kqueue
    int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent changes[USER_LIMIT];
    struct kevent events[USER_LIMIT];

    //HTTP http[100];

    EV_SET(changes,listenfd,EVFILT_READ,EV_ADD | EV_ENABLE | EV_ERROR,0,0,&listenfd);

    if (kevent(kq, changes, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }


    while (1) {

        int nev = kevent(kq, NULL, 0, events, user_count+1, NULL);

        if (nev == -1) {
            perror("kevent:");
            exit(1);
        }

        printf("nums of events:%d\n", nev);

        for (int i = 0; i < nev; ++i) {

            int event_fd = events[i].ident;

            if (events[i].flags & EV_EOF) {
                printf("Client disconnected\n");
                close(event_fd);
            }

            else if (event_fd == listenfd) {
                printf("New client come in...\n");

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );

                // accept a connection on a socket
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );

                // check if wrong
                if ( connfd < 0 )
                {
                    printf( "errno is: %d\n", errno );
                    continue;
                }
                
                //setnonblocking( connfd );
                
                ++user_count;
                printf("Now have %d users\n",user_count);

                EV_SET(changes+user_count, connfd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, &connfd);


                if (kevent(kq,changes,user_count+1,NULL,0,NULL) < 0) {
                    perror("kevent:");
                }
            }
            else if (events[i].filter & EVFILT_READ) {
                printf("test\n");
                int connfd = events[i].ident;
                HTTP http(connfd);
                http.recv_message();
                //http[0].init(connfd);
                //http[0].recv_message();
            }
        }
    }

    close( listenfd );
    return 0;
}
