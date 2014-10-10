#!/bin/bash
../cpldtool/cpldtool 1 p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    ./progcsr.sh
    ./clockenb.sh
fi
