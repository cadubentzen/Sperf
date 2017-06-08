CC = g++
CXX = g++
CPPFLAGS = -c -fpic -std=c++11 -Wall -Werror -O2 -I include/
LDFLAGS =
LDLIBS = -fopenmp -pthread
VPATH= src/:include/

all: sperf libs
	mv sperf bin/
	mv libsperf.a lib/
	rm sperfops.o sperf_instr.o sperf.o

sperf: sperfops.o sperf_instr.o sperf.o

libs: libsperf.a

libsperf.a: sperfops.o
	ar r  $@ $?

libsperf.so: sperfops.o
	g++ -shared -o libsperf.so sperfops.o

sperf.o: sperf.cpp

sperfops.o: sperfops.h sperfops.cpp

sperf_instr.o: sperfops.h sperfops.cpp

clean:
	rm bin/sperf
	rm lib/libsperf.a
