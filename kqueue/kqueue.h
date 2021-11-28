#ifndef KQUEUE_H
#define KQUEUE_H

#include <sys/event.h>

#include <cerrno>
#include <cstdio>

void addfd(int kq, int fd);
void removefd(int kq, int fd);

void addsignal(int kq, int signal);
void removesignal(int kq, int signal);

int trigger(int kq, struct kevent* changelist, int nchanges);

#endif

