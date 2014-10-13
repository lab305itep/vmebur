#!/bin/bash
../cpldtool/cpldtool 1 p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    sleep 2
    ./progcsr.sh
    ./clockenb.sh
else
    ../cpldtool/cpldtool 1 p
fi
