#!/bin/bash

# Compiling the library and profiler

cd src

g++ -c -Wall -Werror -fpic -fopenmp -o sprofops.o sprofops.c
g++ -shared -o libsprof.so sprofops.o

rm sprofops.o
mv libsprof.so ../lib

g++ -Wall -o sprof sprof.c
mv sprof ../bin
