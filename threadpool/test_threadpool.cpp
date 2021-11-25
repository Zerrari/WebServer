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
    threadpool<Test> thread(4,10);
    Test* test1 = new Test;
    Test* test2 = new Test;
    Test* test3 = new Test;
    thread.append(test1);
    thread.append(test2);
    thread.append(test3);
    delete test1;
    delete test2;
    delete test3;
    return 0;
}

