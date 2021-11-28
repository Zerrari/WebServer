#ifndef PROCESS_POOL_H
#define PROCESS_POOL_H

#include "../kqueue/kqueue.h"

#include <cstdio>
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <unistd.h>
#include <fcntl.h>

#include <csignal>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cassert>

// 定义子进程类
class process
{
public:
    process() : m_pid( -1 ){}

public:
    // 定义进程号
    pid_t m_pid;
    // 定义管道
    int m_pipefd[2];
};

// 定义进程池模板类
template< typename T >
class processpool
{
private:
    // 将构造函数封装为私有
    processpool( int listenfd, int _kq, int process_number = 8 );
public:
    static processpool<T>* create(int listenfd, int _kq, int process_number = 8)
    {
        if( !m_instance )
        {
            m_instance = new processpool<T>(listenfd, _kq, process_number);
        }
        return m_instance;
    }
    ~processpool()
    {
        delete [] m_sub_process;
        close(kq);
    }
    void run();

private:
    void run_parent();
    void run_child();

private:
    int m_listenfd;
    // 最大的进程数
    static const int MAX_PROCESS_NUMBER = 16;
    static const int USER_PER_PROCESS = 65536;
    // 最大事件数
    static const int MAX_EVENT_NUMBER = 10000;
    // kqueue fd
    int kq;
    // 进程池中的进程数量
    int m_process_number;
    // 子进程在进程池中的序号
    int m_idx;
    int m_stop;
    process* m_sub_process;
    static processpool<T>* m_instance;
};

template<typename T>
processpool<T>* processpool<T>::m_instance = NULL;

static int setnonblocking(int fd)
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

template<typename T>
processpool<T>::processpool(int listenfd, int _kq, int process_number) 
    : m_listenfd( listenfd ), kq(_kq), m_process_number( process_number ), m_idx( -1 ), m_stop( false )
{
    assert( ( process_number > 0 ) && ( process_number <= MAX_PROCESS_NUMBER ) );

    m_sub_process = new process[ process_number ];
    assert( m_sub_process );

    for( int i = 0; i < process_number; ++i )
    {
        int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd );
        assert( ret == 0 );

        m_sub_process[i].m_pid = fork();
        assert( m_sub_process[i].m_pid >= 0 );

        // 父进程
        if (m_sub_process[i].m_pid > 0)
        {
            // 关闭读端
            close( m_sub_process[i].m_pipefd[1] );
            continue;
        }
        // 子进程
        else
        {
            // 关闭写端
            close(m_sub_process[i].m_pipefd[0]);

            // 设置当前子进程的序号
            m_idx = i;

            printf("create %dth process\n",i);

            // 子进程中断循环，防止fork过多的进程
            break;
        }
    }
}

template<typename T>
void processpool<T>::run()
{
    // 判断是否是子进程
    if(m_idx != -1)
    {
        run_child();
        return;
    }
    run_parent();
}

template<typename T>
void processpool<T>::run_child()
{

    // 监听管道的读端
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    printf("child\n");
    addfd(kq, pipefd);

    struct kevent events[MAX_EVENT_NUMBER];
    T* users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;

    while (!m_stop)
    {
        //number = trigger(kq,events,MAX_EVENT_NUMBER);
        number = kevent(kq, NULL, 0, events, MAX_EVENT_NUMBER, NULL);

        if ((number < 0) && (errno != EINTR))
        {
            perror("child");
            printf("errno: %d\n", errno);
            printf("kqueue failure\n");
            break;
        }

        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].ident;
            if ((sockfd == pipefd) && (events[i].filter == EVFILT_READ))
            {
                int client = 0;
                
                // 接受管道传来的数据
                ret = recv(sockfd, (char*)&client, sizeof(client), 0);
                if(((ret < 0) && (errno != EAGAIN)) || ret == 0) 
                {
                    continue;
                }
                else
                {
                    struct sockaddr_in client_address;
                    socklen_t client_addrlength = sizeof(client_address);
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                    if ( connfd < 0 )
                    {
                        printf( "errno is: %d\n", errno );
                        continue;
                    }
                    setnonblocking(connfd);
                    addfd(kq,connfd);
                    users[connfd].init(connfd,kq);
                }
            }
            // 处理信号
            else if(events[i].filter == EVFILT_SIGNAL)
            {
            }
            else if( events[i].filter == EVFILT_READ )
            {
                 users[sockfd].process();
            }
            else
            {
                continue;
            }
        }
    }

    delete [] users;
    users = NULL;
    close( pipefd );
}

template< typename T >
void processpool< T >::run_parent()
{
    
    setnonblocking(m_listenfd);
    addfd(kq, m_listenfd);

    printf("parent: success\n");

    struct kevent events[MAX_EVENT_NUMBER];

    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    //int ret = -1;

    while(!m_stop)
    {
        number = trigger(kq,events,MAX_EVENT_NUMBER);
        if ((number < 0) && (errno != EINTR))
        {
            perror("parent failure\n");
            printf("kqueue failure\n");
            break;
        }

        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].ident;
            // 有新连接请求到达
            if( sockfd == m_listenfd )
            {
                int i =  sub_process_counter;
                do
                {
                    if( m_sub_process[i].m_pid != -1 )
                    {
                        break;
                    }
                    i = (i+1) % m_process_number;
                }
                while( i != sub_process_counter );
                
                if( m_sub_process[i].m_pid == -1 )
                {
                    m_stop = true;
                    break;
                }
                // 调整为下一个可用子进程
                sub_process_counter = (i+1) % m_process_number;

                // 往管道写数据，传递给子进程
                send( m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
                printf( "send request to child %d\n", i );
                //sub_process_counter %= m_process_number;
            }
            else if (events[i].filter == EVFILT_SIGNAL)
            {
            }
            else
            {
                continue;
            }
        }
    }

    //close( m_listenfd );
}

#endif

