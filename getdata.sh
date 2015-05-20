#!/bin/bash
#	Get & process data
#

ba=${1-2}00000
xil=${2-0}
thr=${3-10}

num=`cat .num`
if [ "X"$num == "X" ] ; then
    num=0
fi
num=$(($num + 1))
echo $num > .num

name=data/run_${num}_${xil}

threg=$(( 2*$xil ))025

./vmebur -q "m $ba 200000;x $threg $thr;n $xil 10000 ${name}.dat"
../anal/dat2root ${name}.dat ${name}.root
root -l -b -q "doplots.C(\"$name\")"
evince ${name}.pdf 2>> /dev/null &
