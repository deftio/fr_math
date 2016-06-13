#!/bin/bash

printf "***************************************************************\n"
printf "RUN + COMPILE SCRIPT HEADER ***********************************\n"

#build it
make

#clean up *.o
make clean

#run it
./FR_main

