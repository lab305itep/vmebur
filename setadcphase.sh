#!/bin/bash
#
for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F ; do
    ./vmebur -q "m 0 2000000;g $i 16=$1"
done
