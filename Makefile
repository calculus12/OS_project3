CC = g++
CXXFLAGS = -Wall -std=c++17
OBJS = main.o Run.o Syscall.o System.o Fault.o

all: main

main : $(OBJS)
	$(CC) $(CXXFLAGS) -o project3 $(OBJS)

System.o : System.hpp System.cpp Error.hpp
	$(CC) $(CXXFLAGS) -c System.cpp

Run.o : Run.cpp Run.hpp
	$(CC) $(CXXFLAGS) -c Run.cpp

Syscall.o : Syscall.cpp Syscall.hpp Error.hpp
	$(CC) $(CXXFLAGS) -c Syscall.cpp

Fault.o : Fault.cpp Fault.hpp
	$(CC) $(CXXFLAGS) -c Fault.cpp

main.o : main.cpp Run.o
	$(CC) $(CXXFLAGS) -c main.cpp

clean:
	rm -f project3 *.o