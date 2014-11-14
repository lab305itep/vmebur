#!/bin/bash
./vmebur -sA16 -wD16 -q "m A000 2000;18=82"
../cpldtool/cpldtool 1 p main.bin
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
    ../cpldtool/cpldtool 1 p
fi
