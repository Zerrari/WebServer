#ifndef HTTP_H
#define HTTP_H

#include <cstdio>
#define BUFFER_SIZE 4096

#include <sys/types.h>
#include <sys/event.h>
#include <netinet/in.h>
#include "stdlib.h"

int addfd(int kqueue_fd, int fd);
int removefd(int kqueue_fd, int fd);

class HTTP{
public:
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 2048;
    // 请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 报文检查状态
    enum CHECK_STATE 
    { 
        CHECK_STATE_REQUESTLINE = 0, 
        CHECK_STATE_HEADER, 
        CHECK_STATE_CONTENT 
    };
    // 行状态
    enum LINE_STATUS 
    { 
        LINE_OK = 0, 
        LINE_BAD, 
        LINE_OPEN 
    };
    // HTTP返回码
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };

public:
    HTTP() {}
    ~HTTP() {}

private:
    int read_once();

    LINE_STATUS parse_line();

    HTTP_CODE process_read();

    HTTP_CODE parse_request_line(char*);
    HTTP_CODE parse_headers(char*);
    HTTP_CODE parse_content(char*);

    HTTP_CODE do_request();


    char* get_line() { return read_buf + start_line; }

public:
    void init(int socket_fd, const sockaddr_in &addr);
    void init(int socket_fd);
    void process() { process_read(); }
    static int get_user_count() { return user_count; }
    void close_connect(bool);

    void static set_kqueue(int fd) {
        kqueue_fd = fd;
    }

public:
    static int kqueue_fd;
    static int user_count;

private:
    void init();

    // HTTP连接的文件描述符
    int connfd;
    sockaddr_in address;
    int content_length;
    bool linger;
    // 请求报文
    char read_buf[READ_BUFFER_SIZE];
    // 读取标记
    int read_idx;
    // 检查标记
    int checked_idx;
    // 请求行开始的标记
    int start_line;
    char write_buf[WRITE_BUFFER_SIZE];
    int write_idx;
    // 解析报文的状态
    CHECK_STATE check_state;
    // HTTP请求报文的方法
    METHOD method;
    // HTTP请求报文的头部
    char *header;
    // HTTP请求报文的url
    char* url;
    // HTTP请求的版本
    char* version;
    // HTTP请求的主机名
    char* host;
};

#endif
