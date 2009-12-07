#!/bin/sh

gcc -o server -w -lsocket -lnsl server.c;
./server -t 3003 -u 4001;

