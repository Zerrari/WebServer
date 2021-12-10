#ifndef intIME_HEAP
#define intIME_HEAP

#include <iostream>
#include <netinet/in.h>
#include <time.h>

struct client_data;

using std::exception;

class heap_timer
{
public:
    heap_timer(int delay)
    {
        expire = time(NULL) + delay;
    }

public:
   time_t expire;
   void (*cb_func)(client_data*);
   client_data* user_data;
};

class time_heap
{
public:
    time_heap(int cap) throw (std::exception)
        :capacity(cap), cur_size(0)
    {
	array = new heap_timer* [capacity];
	if (!array)
	{
            throw std::exception();
	}
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = NULL;
        }
    }
    time_heap(heap_timer** init_array, int size, int capacity) throw (std::exception)
        :capacity(capacity),cur_size(size)
    {
        if ( capacity < size )
        {
            throw std::exception();
        }
        array = new heap_timer* [capacity];
        if (!array)
        {
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i)
        {
            array[i] = NULL;
        }
        if ( size != 0 )
        {
            for (int i =  0; i < size; ++i)
            {
                array[i] = init_array[i];
            }
            for (int i = (cur_size-1)/2; i >=0; --i)
            {
                percolate_down(i);
            }
        }
    }
    ~time_heap()
    {
        for (int i =  0; i < cur_size; ++i)
        {
            delete array[i];
        }
        delete [] array; 
    }

public:
    heap_timer* add_timer(int timeout) throw (std::exception)
    {
        heap_timer* timer = new heap_timer(timeout);

        if(!timer)
        {
            return NULL;
        }

        if(cur_size >= capacity)
        {
            resize();
        }

        int hole = cur_size++;
        int parent = 0;

        for(; hole > 0; hole=parent)
        {
            parent = (hole-1)/2;
            if (array[parent]->expire <= timer->expire)
            {
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
        return timer;
    }

    void del_timer(heap_timer* timer)
    {
        if(!timer)
        {
            return;
        }
        // lazy delelte
        timer->cb_func = NULL;
    }

    void adjust_timer(heap_timer* timer, int timeout)
    {
        timer->expire = time(NULL) + timeout;
    }

    heap_timer* top() const
    {
        if (empty())
        {
            return NULL;
        }
        return array[0];
    }
    void pop_timer()
    {
        if(empty())
        {
            return;
        }
        if(array[0])
        {
            delete array[0];
            array[0] = array[--cur_size];
            percolate_down(0);
        }
    }

    void tick()
    {
        heap_timer* tmp = array[0];
        time_t cur = time(NULL);
        printf("tick once\n");
        while (!empty())
        {
            if (!tmp)
            {
                break;
            }
            if (tmp->expire > cur)
            {
                break;
            }
            if (array[0]->cb_func)
            {
                array[0]->cb_func(array[0]->user_data);
            }
            pop_timer();
            tmp = array[0];
        }
    }

    bool empty() const { return cur_size == 0; }

private:
    void percolate_down(int hole)
    {
        heap_timer* temp = array[hole];
        int child = 0;
        for (; ((hole*2+1) <= (cur_size-1)); hole=child)
        {
            child = hole*2+1;
            if ((child < (cur_size-1)) && (array[child+1]->expire < array[child]->expire))
            {
                ++child;
            }
            if (array[child]->expire < temp->expire)
            {
                array[hole] = array[child];
            }
            else
            {
                break;
            }
        }
        array[hole] = temp;
    }
    void resize() throw ( std::exception )
    {
        heap_timer** temp = new heap_timer* [2*capacity];
        for (int i = 0; i < 2*capacity; ++i)
        {
            temp[i] = NULL;
        }
        if (!temp)
        {
            throw std::exception();
        }
        capacity = 2*capacity;
        for ( int i = 0; i < cur_size; ++i )
        {
            temp[i] = array[i];
        }
        delete [] array;
        array = temp;
    }

private:
    heap_timer** array;
    int capacity;
    int cur_size;
};

#endif
