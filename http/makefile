CXX ?= g++

CXXFLAGS = -Wall -O2

all: http kqueue

http: test_http.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o http

kqueue: http_kqueue.cpp ../sock/sock.cpp http.cpp
	$(CXX) $^ $(CXXFLAGS) -o kqueue

clean:
	rm http kqueue


