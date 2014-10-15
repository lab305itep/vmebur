#!/bin/bash
#	Soft reset all ADCs
#
./vmebur -q "m 0 2000000;g 0 D=$1;g 1 D=$1;g 2 D=$1;g 3 D=$1;g 4 D=$1;g 5 D=$1;g 6 D=$1;g 7 D=$1;"
./vmebur -q "m 0 2000000;g 8 D=$1;g 9 D=$1;g A D=$1;g B D=$1;g C D=$1;g D D=$1;g E D=$1;g F D=$1;"
for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F ; do
    ./vmebur -q "m 0 2000000;x 3=$i;x 2003=$i;x 4003=$i;x 6003=$i;10004=f0000;10004=0"
    ./vmebur -q "m 0 2000000;p A0000 400;p 80000 400;p 60000 400;p 40000 400"
done
