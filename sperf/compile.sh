#!/bin/bash

# Compiling the library and profiler

cd src

g++ -c -Wall -Werror -fpic -fopenmp -pthread -o sperfops.o sperfops.c
ar rcs libsperf.a sperfops.o
g++ -shared -o libsperf.so sperfops.o

rm sperfops.o
mv libsperf.so ../lib
mv libsperf.a ../lib

g++ -Wall -o sperf sperf.c
mv sperf ../bin
