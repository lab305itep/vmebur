#!/bin/bash
../cpldtool/cpldtool 1 p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    sleep 1
    echo Sleeping...
    sleep 1
    echo Sleeping...
    sleep 1
    echo Sleeping...
    sleep 1
    ./progcsr.sh
    sleep 1
    ./clockenb.sh
else
    ../cpldtool/cpldtool 1 p
fi
