#!/usr/bin/env bash

printf "***************************************************************\n"
printf "RUN + COMPILE SCRIPT HEADER ***********************************\n"

#clean up *.o
make clean

#build it
make


#run it
./FR_math_example1.exe

