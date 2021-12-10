CXX ?= g++

CXXFLAGS = -Wall -O2

all: server server1

server: webserver.cpp http/http.cpp sock/sock.cpp
	$(CXX) $^ $(CXXFLAGS) -o server

server1: server.cpp http/http.cpp sock/sock.cpp
	$(CXX) $^ $(CXXFLAGS) -o server1

clean:
	rm server server1
