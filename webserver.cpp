#include "http/http.h"
#include "threadpool/threadpool.h"
#include "sock/sock.h"
#include "timer/lst_timer.h"

#include <ctime>
#include <cassert>
#include <ios>
#include <strings.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <signal.h>

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 65535
#define TIMESLOT 5

HTTP* users = new HTTP[MAX_EVENT_NUMBER];

typedef util_timer Timer;
typedef sort_timer_lst TimerContainer;

static TimerContainer tc;

int addfd(int kqueue_fd, int fd);
int modfd(int kqueue_fd, int fd, int EVFILT_FLAG, int flag, void* udata);

// 定时器回调函数
void cb_func(client_data* user_data)
{
    //users[user_data->sockfd].close_connect(true);
}

// 添加信号处理函数
void addsig(int sig, bool restart = true)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));

    // 使用忽略信号的处理方式
    sa.sa_handler = SIG_IGN;

    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_handler()
{
    tc.tick();
    alarm(TIMESLOT);
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

    // 创建监听套接字描述符
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

    // 产生kqueue队列的描述符
    static int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    // 存储需要监控事件的数组
    struct kevent newevent[3];
    // 存储就绪事件的数组
    struct kevent events[MAX_EVENT_NUMBER];

    // 为信号SIGALRM注册函数
    addsig(SIGALRM, false);
    // 为信号SIGTERM注册函数
    addsig(SIGTERM, false);

    // 设置相关事件的响应方法
    EV_SET(newevent, listenfd, EVFILT_READ, EV_ADD, 0, 0, &listenfd);
    EV_SET(newevent+1, SIGALRM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    EV_SET(newevent+2, SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);

    // 添加套接字描述符，信号SIGALRM，信号SIGTERM到kqueue队列
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

        printf("nev: %d\n", nev);

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
                Timer* timer = user_timer[fd].timer;
                timer->cb_func(&user_timer[fd]);

                if (timer)
                    tc.del_timer(timer);
            }
            else if (fd == listenfd)
            {
                //printf("listen\n");
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);

                // 返回一个描述符用来处理已完成的连接
                int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);

                if (connfd < 0)
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }

                // 设置描述符为非阻塞的
                setnonblocking(connfd);

                users[connfd].init(connfd);

                user_timer[connfd].sockfd = connfd;

                time_t cur = time(NULL);
                time_t timeout = cur + 3 * TIMESLOT;
                Timer* timer = tc.add_timer(timeout);
                timer->cb_func = cb_func;
                timer->user_data = &user_timer[connfd];
                user_timer[connfd].timer = timer;

                printf("Now have %d clients\n", HTTP::get_user_count());
            }
            else if (events[i].filter == EVFILT_READ) {
                //printf("read\n");
                Timer* timer = user_timer[fd].timer;
                int connfd = events[i].ident;

                if (users[connfd].read_once()) {
                    pool->append(users + connfd);

                    //if (timer) {
                        //time_t cur = time(NULL);
                        //timer->expire = cur + 3 * TIMESLOT;
                        //tc.adjust_timer(timer, timer->expire);
                    //}
                }
                //else {
                    //timer->cb_func(&user_timer[fd]);
                    //if (timer) {
                        //tc.del_timer(timer);
                    //}
                //}

            }
            //else if (events[i].filter == EVFILT_WRITE) {
                //char* write_buffer = (char*)events[i].udata;
                //if (write_buffer != NULL) {
                    //send(fd, write_buffer, strlen(write_buffer), 0);
                //}
                //modfd(kq, fd, EVFILT_WRITE, EV_DISABLE, NULL);
                //modfd(kq, fd, EVFILT_READ, EV_ENABLE, NULL);
            //}
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
        //if (timeout) {
            //timer_handler();
            //timeout = false;
        //}
        //printf("kevent loop\n");
    }

    close(listenfd);
    return 0;
}

