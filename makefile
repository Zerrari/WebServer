CXX ?= g++

CXXFLAGS = -Wall -O2

all: server

server: webserver.cpp http/http.cpp sock/sock.cpp
	$(CXX) $^ $(CXXFLAGS) -o server

clean:
	rm server
