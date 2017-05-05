CC = g++
CXX = g++
CPPFLAGS = -std=c++11 -Wall -Werror -c -O2 -I include/
LDFLAGS =
LDLIBS = -fpic -fopenmp -pthread
VPATH= src/:include/

all: sperf libs
	mv sperf bin/
	mv libsperf.a lib/
	rm sperfops.o sperf_instr.o sperf.o

sperf: sperfops.o sperf_instr.o sperf.o

libs: libsperf.a

libsperf.a: sperfops.o
	ar r  $@ $?

sperf.o: sperf.cpp

sperfops.o: sperfops.h sperfops.cpp

sperf_instr.o: sperfops.h sperfops.cpp

clean:
	rm bin/sperf
	rm lib/libsperf.a
