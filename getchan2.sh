#!/bin/bash
#	Soft reset all ADCs
#

if [ "x"$1 == "x" ] ; then
    echo Usage ./`basename $0` xil
    exit -1
fi

case $1 in
0 ) mem=40000;;
1 ) mem=60000;;
2 ) mem=80000;;
3 ) mem=A0000;;
esac

./vmebur -q "m 0 2000000;$mem=0" >> /dev/null
./vmebur -q "m 0 2000000;j $mem 400" > aaa.dat
gnuplot aaa.cmd

