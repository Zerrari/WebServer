#include "sock.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

void addsig( int sig )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
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

    int count = 0;

    // create kqueue
    int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent changes[MAX_EVENT_NUMBER];
    struct kevent events[MAX_EVENT_NUMBER];


    EV_SET(changes,listenfd,EVFILT_READ,EV_ADD | EV_ENABLE | EV_ERROR,0,0,&listenfd);

    int ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd );
    assert( ret != -1 );
    setnonblocking( pipefd[1] );

    //EV_SET(changes+1,pipefd[0],EVFILT_READ,EV_ADD | EV_ENABLE | EV_ERROR,0,0,&pipefd[0]);
    EV_SET(changes+1,SIGINT,EVFILT_SIGNAL,EV_ADD | EV_ENABLE | EV_ERROR,0,0,NULL);

    count = 2;

    // add all the interesting signals here
    //addsig( SIGHUP );
    //addsig( SIGCHLD );
    //addsig( SIGTERM );
    addsig( SIGINT );
    bool stop_server = false;

    //if (kevent(kq, changes, 2, NULL, 0, NULL) == -1)
    //{
        //perror("kevent");
        //exit(1);
    //}

    while( !stop_server )
    {
        int nev = kevent(kq, changes, count, events, count, NULL);

        if (nev == -1) {
            perror("kevent:");
            exit(1);
        }

        printf("nums of events:%d\n", nev);

        if ( ( nev < 0 ) && ( errno != EINTR ) )
        {
            printf( "kqueue failure\n" );
            break;
        }
    
        for ( int i = 0; i < nev; i++ )
        {
            int sockfd = events[i].ident;
            if( sockfd == listenfd )
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );
                int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );
                EV_SET(changes+count,listenfd,EVFILT_READ,EV_ADD | EV_ENABLE | EV_ERROR,0,0,&connfd);
                ++count;
            }
            else if (( events[i].ident = EVFILT_SIGNAL )) {
                stop_server = true;
                printf("Recv signal\n");
                //printf("Test\n");
            }
            else if( ( sockfd == pipefd[0] ) && ( events[i].filter & EVFILT_READ ) )
            {
                //int sig;
                //printf("test\n");
                char signals[1024];
                ret = recv( pipefd[0], signals, sizeof( signals ), 0 );
                if( ret == -1 )
                {
                    continue;
                }
                else if( ret == 0 )
                {
                    continue;
                }
                else
                {
                    for( int i = 0; i < ret; ++i )
                    {
                        printf("%c\n",signals[i]);
                    }
                }
            }
            else
            {
            }
        }
    }

    printf( "close fds\n" );
    close( listenfd );
    close( pipefd[1] );
    close( pipefd[0] );
    return 0;
}
