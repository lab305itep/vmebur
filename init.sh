#!/bin/bash
./vmebur -s0 -ms -q "m A000 2000;18=82"
../cpldtool/cpldtool 1 p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    sleep 1
    echo Sleeping...
    sleep 1
    ./progcsr.sh
    ./pwdadc.sh
    ./clockenb.sh
    ./resetadc.sh
    ./settrig.sh
else
    ../cpldtool/cpldtool 1 p
fi
