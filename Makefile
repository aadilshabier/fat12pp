CXXFLAGS=-Wall -Wextra -std=c++17 -pedantic -ggdb
CXX=clang++

fat12++: fat12++.cpp
	$(CXX) $(CXXFLAGS) -o fat12++ fat12++.cpp
