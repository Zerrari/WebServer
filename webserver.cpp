#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sock/sock.h"
#include "kqueue/kqueue.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <signal.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addsig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int connfd, const char* info)
{
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

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

    //struct kevent changes[MAX_EVENT_NUMBER];
    struct kevent changes;
    struct kevent events[MAX_EVENT_NUMBER];
    int flags[MAX_EVENT_NUMBER];
    for (int i = 0; i < MAX_EVENT_NUMBER; ++i) {
        flags[i] = -1;
    }

    HTTP* users = new HTTP[MAX_FD];
    assert(users);
    int user_count = 0;

    EV_SET(&changes,listenfd,EVFILT_READ,EV_ADD,0,0,&listenfd);

    if (kevent(kq, &changes, 1, NULL, 0, NULL) < 0) {
        perror("kevent");
    }

    addsig(SIGPIPE,SIG_IGN);

    user_count = 1;

    while (1) {

        int nev = kevent(kq, NULL, 0, events, MAX_FD, NULL);

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
                close(events[i].ident);
                --user_count;
            }
            else if(sockfd == listenfd)
            {
                struct kevent newevent;
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                setnonblocking(connfd);
                EV_SET(&newevent,connfd,EVFILT_READ,EV_ADD,0,0,&connfd);
                if (kevent(kq, &newevent, 1, NULL, 0, NULL) < 0) {
                    perror("kevent");
                }
                users[connfd].init(connfd,kq);
                ++user_count;
            }
            else if (events[i].filter == EVFILT_READ) {
                int connfd = events[i].ident;
                pool->append(users + connfd);
            }
        }
    }

    printf("close fds\n");
    close(listenfd);
    return 0;
}
