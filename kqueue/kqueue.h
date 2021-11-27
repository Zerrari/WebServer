#ifndef KQUEUE_H
#define KQUEUE_H

#include <sys/_types/_u_short.h>
#include <sys/event.h>

void updateEvent(int ident, short filter, u_short flags, u_int fflags, int data,void* udata);

#endif

