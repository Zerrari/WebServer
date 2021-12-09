#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sock/sock.h"
#include "timer/lst_timer.h"

#include <ctime>
#include <cassert>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <signal.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5

static sort_timer_lst timer_lst;

static int kq = kqueue();

HTTP* users = new HTTP[MAX_EVENT_NUMBER];

void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

void cb_func(client_data* user_data)
{
    users[user_data->sockfd].close_connect(true);
}

//设置信号函数
void addsig(int sig, bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

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

    addsig(SIGALRM, false);

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

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent newevent;
    struct kevent events[MAX_EVENT_NUMBER];

    // add listenfd to kqueue
    EV_SET(&newevent, listenfd, EVFILT_READ, EV_ADD, 0, 0, &listenfd);

    if (kevent(kq, &newevent, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

    EV_SET(&newevent, SIGALRM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

    if (kevent(kq, &newevent, 1, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

    assert(users);

    HTTP::set_kqueue(kq);

    client_data* user_timer = new client_data[MAX_EVENT_NUMBER];
    bool timeout = false;

    alarm(TIMESLOT);

    while (1) {

        //printf("start to kevent\n");
        int nev = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);

        //printf("events: %d\n",nev);

        if (nev == -1) {
            perror("kevent:");
            exit(1);
        }

        if ((nev < 0) && (errno != EINTR))
        {
            printf("kqueue failure\n");
            break;
        }
    
        for (int i = 0; i < nev; i++)
        {
            int sockfd = events[i].ident;
            if (events[i].flags & EV_EOF)
            {
                users[sockfd].close_connect(true);
                util_timer* timer = user_timer[sockfd].timer;
                timer->cb_func(&user_timer[sockfd]);

                if (timer)
                    timer_lst.del_timer(timer);
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
                user_timer[connfd].address = client_address;
                user_timer[connfd].sockfd = connfd;
                util_timer *timer = new util_timer;
                timer->user_data = &user_timer[connfd];
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                user_timer[connfd].timer = timer;
                timer_lst.add_timer(timer);

                printf("Now have %d users\n", HTTP::get_user_count());
            }
            else if (events[i].filter == EVFILT_READ) {
                util_timer* timer = user_timer[sockfd].timer;
                int connfd = events[i].ident;

                if (users[connfd].read_once()) {
                    pool->append(users + connfd);

                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                }
                else {
                    timer->cb_func(&user_timer[sockfd]);
                    if (timer) {
                        timer_lst.del_timer(timer);
                    }
                }

            }
            else if (sockfd == SIGALRM) {
                printf("handle alarm signal\n");
                timeout = true;
            }
            else {
                continue;
            }
        }
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }

    printf("close fds\n");
    close(listenfd);
    return 0;
}
