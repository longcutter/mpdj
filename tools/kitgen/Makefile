# kitgen: renders the MPDJ default kit (16 WAVs). Builds with Apple clang or g++:
#   make -C tools/kitgen && tools/kitgen/kitgen "MPDJ Kit" --preview
CXX ?= c++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra

kitgen: kitgen.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -lm

clean:
	rm -f kitgen

.PHONY: clean
