#include "http.h"

#include <cstring>
#include <cstdio>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

const char* HTTP::szret[] = {"I get a correct result\n", "Something wrong\n"};

HTTP::LINE_STATUS HTTP::parse_line()
{
    char temp;
    for ( ; check_index < read_index; ++check_index )
    {
        //printf("test\n");
        temp = buffer[check_index];
        if (temp == '\r')
        {
            if ((check_index + 1) == read_index)
            {
                return LINE_OPEN;
            }
            else if (buffer[check_index + 1] == '\n')
            {
                buffer[check_index++] = '\0';
                buffer[check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((check_index > 1) &&  buffer[check_index - 1] == '\r')
            {
                buffer[check_index - 1] = '\0';
                buffer[check_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP::HTTP_CODE HTTP::parse_requestline(char* szTemp)
{
    // strpbrk(const chaar* s,const char* charset):
    // locates the first occurrence of any character in charset of s 
    
    //将请求方法和请求资源用'\0'分隔
    char* szURL = strpbrk(szTemp, " \t");
    if (!szURL)
    {
        return BAD_REQUEST;
    }
    *szURL++ = '\0';

    char* szMethod = szTemp;

    // strcasecmp(const char* s1,const char* s2):
    // compares string s1 and s2
    
    // 判断请求方法是否为GET
    if (strcasecmp(szMethod, "GET") == 0)
    {
        printf("The request method is GET\n");
    }
    else
    {
        return BAD_REQUEST;
    }

    //strspn(const char* s,const char* charset):
    //span the string s
    szURL += strspn(szURL, " \t");

    //将HTTP版本和请求报文分隔开
    char* szVersion = strpbrk(szURL, " \t");
    if (!szVersion)
    {
        return BAD_REQUEST;
    }
    *szVersion++ = '\0';

    //判断HTTP的版本
    szVersion += strspn(szVersion, " \t");
    if (strcasecmp(szVersion, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    if (strncasecmp(szURL, "http://", 7) == 0)
    {
        szURL += 7;
        szURL = strchr(szURL, '/');
    }

    if (!szURL || szURL[ 0 ] != '/')
    {
        return BAD_REQUEST;
    }

    //URLDecode( szURL );
    printf("The request URL is: %s\n", szURL);
    c_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP::HTTP_CODE HTTP::parse_headers(char* szTemp)
{
    if (szTemp[0] == '\0')
    {
        return GET_REQUEST;
    }
    else if (strncasecmp(szTemp, "Host:", 5) == 0)
    {
        szTemp += 5;
        szTemp += strspn( szTemp, " \t" );
        printf("the request host is: %s\n", szTemp);
    }
    else
    {
        printf("I can not handle this header\n");
    }

    return NO_REQUEST;
}

HTTP::HTTP_CODE HTTP::parse_content()
{
    LINE_STATUS linestatus = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    while((linestatus = parse_line()) == LINE_OK)
    {
        char* szTemp = buffer + start_line;
        start_line = check_index;
        switch (c_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                retcode = parse_requestline(szTemp);
                if (retcode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:
            {
                retcode = parse_headers(szTemp);
                if (retcode == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if (retcode == GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                break;
            }
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    if(linestatus == LINE_OPEN)
    {
        return NO_REQUEST;
    }
    else
    {
        return BAD_REQUEST;
    }
}

void HTTP::recv_message()
{
    memset(buffer, '\0', BUFFER_SIZE);
    int data_read = 0;
    while( 1 )
    {
        data_read = recv(sockfd, buffer + read_index, BUFFER_SIZE - read_index, 0);
        if (data_read == -1)
        {
            printf("reading failed\n");
            break;
        }
        else if (data_read == 0)
        {
            printf("remote client has closed the connection\n");
            break;
        }

        read_index += data_read;
        HTTP::HTTP_CODE result = parse_content();
        if(result == NO_REQUEST)
        {
            continue;
        }
        else if(result == GET_REQUEST)
        {
            send(sockfd,szret[0], strlen( szret[0] ), 0);
            break;
        }
        else
        {
            send(sockfd, szret[1], strlen( szret[1] ), 0);
            break;
        }
    }
    close(sockfd);
}
