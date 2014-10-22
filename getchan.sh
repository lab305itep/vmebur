#!/bin/bash
#	Soft reset all ADCs
#

if [ "x"$2 == "x" ] ; then
    echo Usage ./`basename $0` xil chan
    exit -1
fi

case $1 in
0 ) mem=A0010; csr=3;;
1 ) mem=80010; csr=2003;;
2 ) mem=60010; csr=4003;;
3 ) mem=40010; csr=6003;;
esac

./vmebur -q "m 0 2000000;x $csr $2;10004=f0000;w;10004=0" >> /dev/null
./vmebur -q "m 0 2000000;j $mem 3F0" > aaa.dat
gnuplot aaa.cmd

