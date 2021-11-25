#include "kqueue.h"
#include <sys/event.h>
#include <sys/times.h>
#include <sys/types.h>

int addfd(struct kevent* changes,int* flags,int fd) {
    int first = 1;
    while (flags[first] == -1) {
        ++first;
    }
    flags[first] = fd;
    EV_SET(changes+first,fd,EVFILT_READ,EV_ADD | EV_ENABLE | EV_ERROR,0,0,&fd);
    return 0;
}

int removefd(struct kevent* changes,int* flags,int fd) {
    int target = 1;
    while (flags[target] == fd) {
        ++target;
    }
    EV_SET(changes+target,fd,EVFILT_READ,EV_DELETE,0,0,&fd);
    flags[target] = -1;
    return 0;
}

