CXX ?= g++

CXXFLAGS = -Wall -O2

all: server stress_test test

server: webserver.cpp http/http.cpp sock/sock.cpp
	$(CXX) $^ $(CXXFLAGS) -o server

stress_test: stress_test.cpp 
	$(CXX) $^ $(CXXFLAGS) -o stress_test

test: test1.cpp 
	$(CXX) $^ $(CXXFLAGS) -o test

clean:
	rm server stress_test test
