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
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

// 储存客户端的数据
struct client_data
{
    sockaddr_in address;
    char* write_buf;
    char buf[ BUFFER_SIZE ];
};

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

    // convert ASCII string to integer
    int port = atoi( argv[2] );

    int ret = 0;

    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;

    // convert ip address to network format
    inet_pton( AF_INET, ip, &address.sin_addr );

    // convert values from host byte order to network byte order
    address.sin_port = htons( port );

    // create a socket for tcp communication
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );

    // bind socket to address(ip + port)
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );

    // listen for connections on a socket
    ret = listen( listenfd, 5 );
    assert( ret != -1 );

    printf("listenfd: %d\n",listenfd);

    // 储存客户端数据的数组
    client_data* users = new client_data[FD_LIMIT];
    int user_count = 0;

    // create kqueue
    int kq = kqueue();

    if (kq == -1) {
        perror("kqueue:");
    }

    struct kevent changes[USER_LIMIT];
    struct kevent events[USER_LIMIT];


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
                
                users[connfd].address = client_address;
                setnonblocking( connfd );
                
                ++user_count;
                printf("Now have %d users\n",user_count);

                EV_SET(changes+user_count,connfd,EVFILT_READ,EV_ADD | EV_ENABLE,0,0,&connfd);

                if (kevent(kq,changes,user_count+1,NULL,0,NULL) < 0) {
                    perror("kevent:");
                }
            }
            else if (events[i].filter & EVFILT_READ) {
                int connfd = events[i].ident;
                memset( users[connfd].buf, '\0', BUFFER_SIZE );
                int ret  = recv(connfd,users[connfd].buf,BUFFER_SIZE-1,0);
                if( ret <= 0 )
                {
                    if( errno != EAGAIN )
                    {
                        close(connfd);
                        --user_count;
                    }
                }
                else {
                    for (int i = 1; i <= user_count; ++i) {
                        if ((int)changes[i].ident == connfd) 
                        {
                            continue;
                        }

                        users[changes[i].ident].write_buf = users[connfd].buf;
                        EV_SET(changes+i,connfd,EVFILT_WRITE,EV_ADD | EV_ENABLE,0,0,&connfd);
                    }
                }
                printf("recv %d bytes\n",ret);
            }
            else if (events[i].filter & EVFILT_WRITE) {

                int connfd = events[i].ident;
                if( ! users[connfd].write_buf )
                {
                    continue;
                }
                // 将数据向用户发送
                ret = send( connfd, users[connfd].write_buf, strlen( users[connfd].write_buf ), 0 );
                users[connfd].write_buf = NULL;
                EV_SET(changes+i,connfd,EVFILT_READ,EV_ADD | EV_ENABLE,0,0,&connfd);
            }
        }
    }

    delete [] users;
    close( listenfd );
    return 0;
}
