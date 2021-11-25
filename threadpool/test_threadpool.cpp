#include <iostream>
#include "threadpool.h"

class Test{
public:
    void process() {
        std::cout << "Count: " << ++count << std::endl;
    }
private:
    static int count;
};

int Test::count = 0;

int main()
{
    threadpool<Test> thread;
    Test* test = new Test;
    if (thread.append(test))
        std::cout << "append request" << std::endl;
    delete test;
    return 0;
}

