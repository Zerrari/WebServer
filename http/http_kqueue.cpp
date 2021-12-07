#include "http.h"
#include "../sock/sock.h"

#include <algorithm>
#include <memory>
#include <sys/_types/_errno_t.h>
#include <sys/errno.h>
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
#define MAX_EVENT_NUMBER 66535

// 设置文件描述符为非阻塞
//int setnonblocking( int fd )
//{
    //int old_option = fcntl( fd, F_GETFL );
    //int new_option = old_option | O_NONBLOCK;
    //fcntl( fd, F_SETFL, new_option );
    //int flags = fcntl(fd, F_GETFL);
    //if ((flags & O_NONBLOCK) == O_NONBLOCK) {
      //printf("Yup, it's nonblocking.\n");
    //}
    //else {
      //printf("Nope, it's blocking.\n");
    //}
    //return old_option;
//}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }


    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int listenfd = create_socket(ip, port);

    printf("listenfd: %d\n", listenfd);


    // create kqueue
    int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent newevent;
    struct kevent events[MAX_EVENT_NUMBER];

    HTTP* users = new HTTP[MAX_EVENT_NUMBER];

    EV_SET(&newevent, listenfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &listenfd);

    if (kevent(kq, &newevent, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

    HTTP::set_kqueue(kq);

    while (1) {
        int nev = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);

        if (nev == -1) {
            perror("kevent:");
            exit(1);
        }

        printf("nums of events:%d\n", nev);

        for (int i = 0; i < nev; ++i) {

            int event_fd = events[i].ident;

            if (events[i].flags & EV_EOF) {
                users[event_fd].close_connect(true);
            }

            else if (event_fd == listenfd) {
                printf("New client come in...\n");

                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

                // accept a connection on a socket
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                // check if wrong
                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                
                printf("connfd: %d\n",connfd);
                users[connfd].init(connfd, client_address);

                printf("Now have %d users\n", HTTP::get_user_count());
            }

            else if (events[i].filter == EVFILT_READ) {
                users[event_fd].process();
            }
            else {
                continue;
            }
        }
    }

    close(listenfd);
    return 0;
}
