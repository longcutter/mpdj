CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall

p8render: p8render.cpp p8sound.cpp p8sound.h
	$(CXX) $(CXXFLAGS) -o $@ p8render.cpp p8sound.cpp

clean:
	rm -f p8render
