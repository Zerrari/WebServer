#ifndef KQUEUE_H
#define KQUEUE_H

#include <sys/event.h>

int addfd(struct kevent*, int*, int);
int removefd(struct kevent*, int*, int);

#endif

