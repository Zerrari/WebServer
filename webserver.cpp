#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sock/sock.h"
#include "timer/lst_timer.h"

#include <ctime>
#include <cassert>
#include <i386/types.h>
#include <sys/_types/_time_t.h>
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

// 双向链表定时器
static sort_timer_lst timer_lst;

HTTP* users = new HTTP[MAX_EVENT_NUMBER];

void timer_handler()
{
    timer_lst.tick();
    alarm(TIMESLOT);
}

// 定时器回调函数
void cb_func(client_data* user_data)
{
    users[user_data->sockfd].close_connect(true);
}

// 添加信号处理函数
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

int main(int argc,char* argv[])
{
    if(argc <= 2)
    {
        printf("usage: %s ip_address port_number\n", argv[0]);
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);
    int listenfd = create_socket(ip,port);

    printf("Server starts to listening...\n");

    threadpool<HTTP>* pool = NULL;
    try
    {
        pool = new threadpool<HTTP>;
    }
    catch( ... )
    {
        return 1;
    }

    static int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent newevent[3];
    struct kevent events[MAX_EVENT_NUMBER];

    // 为信号SIGALRM注册函数
    addsig(SIGALRM, false);
    addsig(SIGTERM, false);

    // add listenfd to kqueue
    EV_SET(newevent, listenfd, EVFILT_READ, EV_ADD, 0, 0, &listenfd);
    EV_SET(newevent+1, SIGALRM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    EV_SET(newevent+2, SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

    if (kevent(kq, newevent, 3, NULL, 0, NULL) == -1)
    {
        perror("kevent");
        exit(1);
    }

    HTTP::set_kqueue(kq);

    client_data* user_timer = new client_data[MAX_EVENT_NUMBER];

    bool timeout = false;
    bool stop_server = false;

    alarm(TIMESLOT);

    while (!stop_server) {

        int nev = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);

        if ((nev < 0) && (errno != EINTR))
        {
            printf("kqueue failure\n");
            break;
        }
    
        for (int i = 0; i < nev; i++)
        {
            int fd = events[i].ident;
            if (events[i].flags & EV_EOF)
            {
                users[fd].close_connect(true);
                util_timer* timer = user_timer[fd].timer;
                timer->cb_func(&user_timer[fd]);

                if (timer)
                    timer_lst.del_timer(timer);
            }
            else if(fd == listenfd)
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

                user_timer[connfd].sockfd = connfd;

                
                time_t cur = time(NULL);
                time_t expire = cur + 3 * TIMESLOT;
                util_timer *timer = new util_timer(expire, cb_func, &user_timer[connfd]);
                user_timer[connfd].timer = timer;
                timer_lst.add_timer(timer);

                printf("Now have %d clients\n", HTTP::get_user_count());
            }
            else if (events[i].filter == EVFILT_READ) {
                util_timer* timer = user_timer[fd].timer;
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
                    timer->cb_func(&user_timer[fd]);
                    if (timer) {
                        timer_lst.del_timer(timer);
                    }
                }

            }
            // 处理SIGALRM信号
            else if (fd == SIGALRM) {
                timeout = true;
            }
            else if (fd == SIGTERM) {
                printf("Sever terminate...\n");
                stop_server = true;
            }
            else {
                continue;
            }
        }
        // 再次调用alarm
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }

    close(listenfd);
    return 0;
}
