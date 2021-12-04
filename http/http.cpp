#include "http.h"
//#include "../kqueue/kqueue.h"

#include <string.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>

#include <iostream>

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    int flags = fcntl(fd, F_GETFL);
    if ((flags & O_NONBLOCK) == O_NONBLOCK) {
      printf("Yup, it's nonblocking.\n");
    }
    else {
      printf("Nope, it's blocking.\n");
    }
    return old_option;
}

// 将文件描述符添加到kqueue队列
int addfd(int kqueue_fd, int fd)
{
    struct kevent newevent[2];

    setnonblocking(fd);

    EV_SET(newevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd);
    EV_SET(newevent+1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, &fd);

    if (kevent(kqueue_fd, newevent, 2, NULL, 0, NULL) == -1) {
        perror("kevent");
        return -1;
    }

    return 0;
}

// 将文件描述符从kqueue队列移除
int removefd(int kqueue_fd, int fd)
{
    struct kevent newevent[2];
    
    EV_SET(newevent, fd, EVFILT_READ, EV_DELETE, 0, 0, &fd);
    EV_SET(newevent+1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, &fd);

    if (kevent(kqueue_fd, newevent, 2, NULL, 0, NULL) == -1) {
        perror("kevent");
        return -1;
    }

    return 0;
}

// 用户个数
int HTTP::user_count = 0;
// kqueue文件描述符
int HTTP::kqueue_fd = -1;

//关闭连接，关闭一个连接，客户总量减一
void HTTP::close_connect(bool real_close)
{
    if (real_close && (connfd != -1))
    {
        printf("Client disconnected\n");

        removefd(kqueue_fd, connfd);

        int ret = close(connfd);
        if (ret < 0) {
            printf("errno: %d\n",errno);
        }

        connfd = -1;
        --user_count;
    }
    //sleep(10);
}

//初始化连接,外部调用初始化套接字地址
void HTTP::init(int socket_fd, const sockaddr_in &addr)
{
    connfd = socket_fd;
    address = addr;
    //int reuse=1;
    //setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(kqueue_fd, socket_fd);
    ++user_count;
    init();
}

void HTTP::init(int socket_fd)
{
    connfd = socket_fd;
    //int reuse=1;
    //setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(kqueue_fd, socket_fd);
    ++user_count;
    init();

}

//初始化新接受的连接
//check_state默认为分析请求行状态
void HTTP::init()
{
    check_state = CHECK_STATE_REQUESTLINE;
    method = GET;
    url = 0;
    version = 0;
    host = 0;
    start_line = 0;
    checked_idx = 0;
    read_idx = 0;
    write_idx = 0;

    // 初始化读写缓冲区
    memset(read_buf, '\0', READ_BUFFER_SIZE);
    memset(write_buf, '\0', WRITE_BUFFER_SIZE);
}

int HTTP::read_once()
{
    if (read_idx >= READ_BUFFER_SIZE)
    {
        return -2;
    }

    int bytes_read = 0;

    bytes_read = recv(connfd, read_buf + read_idx, READ_BUFFER_SIZE - read_idx, 0);
    read_idx += bytes_read;

    return bytes_read;
}

//从状态机，用于分析出一行内容
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
HTTP::LINE_STATUS HTTP::parse_line()
{
    char temp;
    for (; checked_idx < read_idx; ++checked_idx)
    {
        temp = read_buf[checked_idx];

        // 判断换行符
        if (temp == '\r')
        {
            // 检查标记和读取标记重合，继续等待读取行 
            if ((checked_idx + 1) == read_idx)
                return LINE_OPEN;

            else if (read_buf[checked_idx + 1] == '\n')
            {
                // 分割请求报文
                read_buf[checked_idx++] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }

            // 请求报文格式错误
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (checked_idx > 1 && read_buf[checked_idx - 1] == '\r')
            {
                read_buf[checked_idx - 1] = '\0';
                read_buf[checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

//解析http请求行，获得请求方法，目标url及http版本号
HTTP::HTTP_CODE HTTP::parse_request_line(char *text)
{
    url = strpbrk(text, " \t");
    if (!url)
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';
    char *temp_method = text;
    if (strcasecmp(temp_method, "GET") == 0) {
        printf("method: GET\n");
        method = GET;
    }
    else if (strcasecmp(temp_method, "POST") == 0)
    {
        printf("method: POST\n");
        method = POST;
    }
    else
        return BAD_REQUEST;
    url += strspn(url, " \t");
    version = strpbrk(url, " \t");
    if (!version)
        return BAD_REQUEST;
    *version++ = '\0';
    version += strspn(version, " \t");
    if (strcasecmp(version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        url = strchr(url, '/');
    }

    if (strncasecmp(url, "https://", 8) == 0)
    {
        url += 8;
        url = strchr(url, '/');
    }

    if (!url || url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示判断界面
    if (strlen(url) == 1)
        strcat(url, "judge.html");
    check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析http请求头部信息
HTTP::HTTP_CODE HTTP::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (content_length != 0)
        {
            check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        host = text;
    }
    else
    {
        printf("oop!unknow header: %s\n",text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
HTTP::HTTP_CODE HTTP::parse_content(char *text)
{
    if (read_idx >= (content_length + checked_idx))
    {
        text[content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        header = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

//
HTTP::HTTP_CODE HTTP::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    int bytes_read = 0;
    printf("process_read\n");

    while (1)
    {
        bytes_read = recv(connfd, read_buf + read_idx, READ_BUFFER_SIZE - read_idx, 0);

        // error handle
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else 
                perror("errno");
        }

        read_idx += bytes_read;
        
        text = get_line();
        start_line = checked_idx;

        printf("recv %d\n bytes\n", bytes_read);

        switch (check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST) {
                printf("BAD REQUEST\n");
                //return BAD_REQUEST;
            }
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HTTP::HTTP_CODE HTTP::do_request()
{
    printf("do_request\n");
    return FILE_REQUEST;
}
