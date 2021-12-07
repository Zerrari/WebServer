#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sock/sock.h"

#include <ios>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <signal.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

//int setnonblocking(int fd)
//{
    //int old_option = fcntl(fd, F_GETFL);
    //int new_option = old_option | O_NONBLOCK;
    //fcntl(fd, F_SETFL, new_option);
    //return old_option;
//}

int main(int argc,char* argv[])
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

    threadpool<HTTP>* pool = NULL;
    try
    {
        pool = new threadpool<HTTP>;
    }
    catch( ... )
    {
        return 1;
    }

    // create kqueue
    int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent newevent;
    struct kevent events[MAX_EVENT_NUMBER];

    EV_SET(&newevent, listenfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &listenfd);

    if (kevent(kq, &newevent, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

    HTTP* users = new HTTP[MAX_EVENT_NUMBER];
    assert(users);

    HTTP::set_kqueue(kq);

    while (1) {

        int nev = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);

        if (nev == -1) {
            perror("kevent:");
            exit(1);
        }

        //printf("nums of events:%d\n", nev);
        if ((nev < 0) && (errno != EINTR))
        {
            printf("kqueue failure\n");
            break;
        }
    
        for (int i = 0; i < nev; i++)
        {
            int sockfd = events[i].ident;
            if (events[i].flags == EV_EOF)
            {
                users[sockfd].close_connect(true);
            }
            else if(sockfd == listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }

                setnonblocking(connfd);
                
                users[connfd].init(connfd, client_address);

                printf("Now have %d users\n", HTTP::get_user_count());
            }
            else if (events[i].filter == EVFILT_READ) {
                int connfd = events[i].ident;
                pool->append(users + connfd);
            }
            else {
                continue;
            }
        }
    }

    printf("close fds\n");
    close(listenfd);
    return 0;
}
