#!/bin/bash
num=${1-1}
echo NUM=$num
./vmebur -sA16 -wD16 -q "m A000 2000;${num}8=82"
../cpldtool/cpldtool ${num} p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    sleep 1
    echo Sleeping...
    sleep 1
    ./progcsr.sh
    if [ "x"$1 != "x-c" ] ; then
	./pwdadc.sh
	./clockenb.sh
	./resetadc.sh
    fi
    ./settrig.sh
else
    ../cpldtool/cpldtool ${num} p
fi
