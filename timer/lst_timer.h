#ifndef LST_TIMER
#define LST_TIMER

#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>

class util_timer;

struct client_data{
    int sockfd;
    util_timer* timer;
};

class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}
    util_timer(time_t _expire, void(*_cb_func)(client_data*), client_data* _user_data):expire(_expire), cb_func(_cb_func), user_data(_user_data), prev(NULL), next(NULL) {}

public:
    time_t expire;
    void (*cb_func)(client_data *);
    client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

class sort_timer_lst
{
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst()
    {
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }
    util_timer* add_timer(int timeout)
    {
        util_timer* timer = new util_timer();
        timer->expire = timeout;

        if (!timer)
        {
            return NULL;
        }
        if (!head)
        {
            head = tail = timer;
            return timer;
        }
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return timer;
        }
        add_timer(timer, head);
        return timer;
    }
    void adjust_timer(util_timer *timer, int timeout)
    {
        if (!timer)
        {
            return;
        }

        timer->expire = timeout;
        util_timer *tmp = timer->next;

        if (!tmp || (timer->expire < tmp->expire))
        {
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }
    void del_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
    void tick()
    {
        if (!head)
        {
            return;
        }
        time_t cur = time(NULL);
        util_timer *tmp = head;
        while (tmp)
        {
            if (cur < tmp->expire)
            {
                break;
            }
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head)
            {
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if (!tmp)
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};

#endif
