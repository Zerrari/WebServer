#include "http.h"

#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/uio.h>

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

//对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    int flags = fcntl(fd, F_GETFL);
    if ((flags & O_NONBLOCK) == O_NONBLOCK) {
      //printf("Yup, it's nonblocking.\n");
    }
    else {
      //printf("Nope, it's blocking.\n");
    }
    return old_option;
}

// 将文件描述符添加到kqueue队列
int addfd(int kqueue_fd, int fd)
{
    struct kevent newevent[2];

    EV_SET(newevent, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, &fd);
    EV_SET(newevent+1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, &fd);

    if (kevent(kqueue_fd, newevent, 2, NULL, 0, NULL) == -1) {
        perror("kevent");
        return -1;
    }

    return 0;
}

int modfd(int kqueue_fd, int fd, int EVFILT_FLAG, int flag, void* data)
{
    struct kevent newevent;

    EV_SET(&newevent, fd, EVFILT_FLAG, flag, 0, 0, data);

    if (kevent(kqueue_fd, &newevent, 1, NULL, 0, NULL) == -1) {
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
    //setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
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
    // strpbrk:
    // 输出第二个字符串的任意字符第一次在第一个字符串的位置，如果没有，返回空指针
    url = strpbrk(text, " \t");
    if (!url)
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';
    char *temp_method = text;
    // strcasecmp
    // 比较字符串的大小
    if (strcasecmp(temp_method, "GET") == 0) {
        //printf("method: GET\n");
        method = GET;
    }
    else if (strcasecmp(temp_method, "POST") == 0)
    {
        //printf("method: POST\n");
        method = POST;
    }
    else
        return BAD_REQUEST;
    // strspn:
    // 返回第一个字符串中没有在第二个字符串中出现的第一个字符索引
    url += strspn(url, " \t");
    version = strpbrk(url, " \t");
    if (!version)
        return BAD_REQUEST;
    *version++ = '\0';
    version += strspn(version, " \t");

    if (strncasecmp(version, "HTTP/1.1", 8) != 0) {
        return BAD_REQUEST;
    }
    //printf("version: %s\n", version);

    if (strncasecmp(url, "http://", 7) == 0)
    {
        url += 7;
        // strchr
        // 寻找第二个字符第一次出现的位置，如果没有，返回空指针
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
    if (strlen(url) == 1) {
        // strcat
        // 合并字符串，返回第一个字符串
        strcat(url, "judge.html");
    }
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
            //printf("state\n");
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
            //printf("Connection: keep-alive\n");
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        content_length = atol(text);
        //printf("Content-length: %d\n",content_length);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        host = text;
        //printf("Host: %s\n", host);
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
        header = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HTTP::HTTP_CODE HTTP::process_read()
{
    HTTP::LINE_STATUS linestatus = LINE_OK;
    HTTP::HTTP_CODE retcode = NO_REQUEST;

    while ((linestatus = parse_line()) == LINE_OK && retcode == NO_REQUEST)
    {
        char* szTemp = read_buf + start_line;
        start_line = checked_idx;
        switch (check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                retcode = parse_request_line(szTemp);
                if (retcode == BAD_REQUEST)
                {
                    printf("BAD_REQUEST\n");
                    init();
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                retcode = parse_headers(szTemp);
                if (retcode == BAD_REQUEST)
                {
                    printf("BAD_REQUEST\n");
                    init();
                    return BAD_REQUEST;
                }
                else if (retcode == GET_REQUEST)
                {
                    printf("GET_REQUEST\n");
                    init();
                    return GET_REQUEST;
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                retcode = parse_content(szTemp);
                if (retcode == GET_REQUEST) {
                    printf("GET_REQUEST\n");
                    init();
                    return GET_REQUEST;
                }
                break;
            }

            default:
            {
                init();
                return INTERNAL_ERROR;
            }
        }
    }
    
    return NO_REQUEST;
}

int HTTP::read_once()
{
    int bytes_read = recv(connfd, read_buf + read_idx, READ_BUFFER_SIZE - read_idx, 0);

    if (bytes_read > 0) 
        read_idx += bytes_read;

    printf("read_idx: %d\n", read_idx);
    
    return bytes_read;
}

void HTTP::process()
{
    //printf("start\n");
    process_read();
    //printf("exit\n");
}

void HTTP::restart()
{
    printf("restart\n");
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

void HTTP::do_request()
{
    modfd(kqueue_fd, connfd, EVFILT_READ, EV_DISABLE, NULL);
    write_buffer = "do request\n";
    modfd(kqueue_fd, connfd, EVFILT_WRITE, EV_ENABLE, write_buffer);
    printf("Do request\n");
    //char response[2048];
    //int fd = open("./../index.html", O_RDONLY | O_NONBLOCK);
    //read(fd, response, 2048);
    //send(connfd, response, 2048, 0);
}
