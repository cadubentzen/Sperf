#!/bin/bash

# Compiling the library and profiler

cd src

g++ -c -Wall -Werror -fpic -fopenmp -pthread -o sperfops.o sperfops.cpp
ar rcs libsperf.a sperfops.o
g++ -shared -o libsperf.so sperfops.o

rm sperfops.o
mv libsperf.so ../lib
mv libsperf.a ../lib

g++ -std=c++11 -Wall -o sperf sperf.cpp
mv sperf ../bin
