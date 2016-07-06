#!/bin/bash
g++ -std=c++11 \
    -O3 \
    -I/usr/local/lib \
    -L/usr/local/lib \
    -lsfml-system \
    -lsfml-graphics \
    -lsfml-window \
    -lsfml-audio \
    -lkeyfinder \
    -lfftw3 \
    -o $1 \
    "./$1.cpp" \
&& ./$1
