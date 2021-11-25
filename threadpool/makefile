CXX ?= g++

CXXFLAGS = -Wall -O2

all: test_threadpool

test_http:	test_threadpool.cpp threadpool.cpp 
	$(CXX) $^ $(CXXFLAGS) -o test_threadpool


clean:
	rm test_threadpool


