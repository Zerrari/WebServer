CXX ?= g++

CXXFLAGS = -Wall -O2

all: test_http http_kqueue

test_http: test_http.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o test_http

http_kqueue: http_kqueue.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o http_kqueue

clean:
	rm test_http http_kqueue


