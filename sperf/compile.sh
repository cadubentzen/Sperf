#!/bin/bash

# Compiling the library and profiler

cd src

g++ -c -Wall -Werror -fpic -fopenmp -o sperfops.o sperfops.c
g++ -shared -o libsperf.so sperfops.o

rm sperfops.o
mv libsperf.so ../lib

g++ -Wall -o sperf sperf.c
mv sperf ../bin
