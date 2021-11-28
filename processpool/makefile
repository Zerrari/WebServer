CXX ?= g++

CXXFLAGS = -Wall -O2

all: processpool

processpool: test_processpool.cpp ../sock/sock.cpp ../http/http.cpp ../kqueue/kqueue.cpp
	$(CXX) $^ $(CXXFLAGS) -o processpool


clean:
	rm processpool
