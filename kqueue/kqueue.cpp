#include "kqueue.h"
#include <cstdio>
#include <sys/event.h>

void addfd(int kq,int fd) {

    struct kevent newevents[2];
    EV_SET(newevents,fd,EVFILT_READ,EV_ADD | EV_ENABLE,0,0,&fd);
    EV_SET(newevents+1,fd,EVFILT_WRITE,EV_ADD | EV_DISABLE,0,0,&fd);

    if (kevent(kq, newevents, 2, NULL, 0, NULL) < 0) {
        perror("kevent");
    }
     
}

void removefd(int kq,int fd) {
    struct kevent newevents[2];
    EV_SET(newevents,fd,EVFILT_READ,EV_DELETE,0,0,&fd);
    EV_SET(newevents+1,fd,EVFILT_WRITE,EV_DELETE,0,0,&fd);

    if (kevent(kq,newevents,2,NULL, 0, NULL) < 0)
        perror("kevent");

}

void addsignal(int kq,int signal) {

    struct kevent newevent;
    EV_SET(&newevent,signal,EVFILT_SIGNAL,EV_ADD,0,0,0);

    if (kevent(kq,&newevent,1,NULL,0,NULL) < 0)
        perror("kevent");
}

void removesignal(int kq,int signal) {

    struct kevent newevent;
    EV_SET(&newevent,signal,EVFILT_SIGNAL,EV_DELETE,0,0,0);

    if (kevent(kq,&newevent,1,NULL,0,NULL) < 0)
        perror("kevent");
}

int trigger(int kq,struct kevent* changelist, int nchanges) {
    int ret = kevent(kq,NULL,0,changelist,nchanges,NULL);
    if (ret < 0) {
        perror("trigger");
    }
    return ret;
}
