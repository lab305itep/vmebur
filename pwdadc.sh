#!/bin/bash
#	Power down all ADCs
#
ba=$1
if [ $ba"x" == "x" ] ; then ba="0" ; fi 
./vmebur -q "m $ba 2000000;g 0 8=1;g 1 8=1;g 2 8=1;g 3 8=1;g 4 8=1;g 5 8=1;g 6 8=1;g 7 8=1;"
./vmebur -q "m $ba 2000000;g 8 8=1;g 9 8=1;g A 8=1;g B 8=1;g C 8=1;g D 8=1;g E 8=1;g F 8=1;"
