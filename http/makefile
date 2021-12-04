CXX ?= g++

CXXFLAGS = -Wall -O2

all: test_http test_kqueue

test_http: test_http.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o test_http

test_kqueue: http_kqueue.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o test_kqueue

clean:
	rm test_http test_kqueue


