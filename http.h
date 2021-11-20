#ifndef HTTP_H
#define HTTP_H

#define BUFFER_SIZE 4096

#include <cstdio>

class HTTP{
public:
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION };
    static const char* szret[];

public:
    HTTP(int _sockfd):sockfd(_sockfd){
        buffer = new char[BUFFER_SIZE];
        c_state = CHECK_STATE_REQUESTLINE;
        l_status = LINE_OPEN;
        h_code = NO_REQUEST;
        check_index = 0;
        read_index = 0;
        start_line = 0;
    }
    ~HTTP(){
        //if (buffer) {
            delete [] buffer;
        //}
    }

private:
    LINE_STATUS parse_line();
    HTTP_CODE parse_requestline(char*);
    HTTP_CODE parse_headers(char*);

public:
    HTTP_CODE parse_content();
    void recv_message();
    void init(int _sockfd) {
        sockfd = _sockfd;
        buffer = new char[BUFFER_SIZE]();
    }

private:
    char* buffer;
    int sockfd;
    CHECK_STATE c_state;
    HTTP_CODE h_code;
    LINE_STATUS l_status;
    int check_index;
    int read_index;
    int start_line;
};


#endif

