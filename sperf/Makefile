CC = g++
CXX = g++
CPPFLAGS = -c -fpic -std=c++11 -Wall -Werror -O2 -I include/
LDFLAGS =
LDLIBS = -fopenmp -pthread
VPATH= src/:include/

all: sperf
	mv sperf bin/
	rm sperf_instr.o sperf.o

sperf: sperf_instr.o sperf.o

sperf.o: sperf.cpp

sperf_instr.o: sperf_instr.cpp

clean:
	rm bin/sperf
