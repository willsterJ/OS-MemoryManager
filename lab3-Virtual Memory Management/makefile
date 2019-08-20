# Makefile 

# Variables to control Makefile operation

CXX = g++
CXXFLAGS = -Wall -g -std=c++11

# ****************************************************
# Targets needed to bring the executable up to date
default: mmu

mmu: Source.o MemManager.o
	$(CXX) $(CXXFLAGS) -o mmu Source.o MemManager.o

Source.o: Source.cpp MemManager.h global.h
	$(CXX) $(CXXFLAGS) -c Source.cpp

MemManager.o: MemManager.cpp MemManager.h global.h
	$(CXX) $(CXXFLAGS) -c MemManager.cpp

clean:
	rm -rf *.o
	rm -rf mmu