#!/bin/bash
num=${1-1}
ga=${2-$num}
printf -v ba "%X" $(( 0x$ga << 20))
echo Trying module serial=0x$num with geographical 0x$ga, base addr is 0x$ba
printf -v dnum "%d" $(( 0x$num ))

# calc parity
lga=${ga:(-1):1}
hga=${ga:(-2):1}
if [ "x"$hga == "x" ] ; then hga="0" ; fi

case $lga in
0 | 3 | 5 | 6 | 9 | A | C | F | a | c | f) p=1;;
* ) p=0;;
esac

case $hga in
0 ) if [ $p == "1" ] ; then val="A"$lga ; else val="8"$lga ; fi;;
1 ) if [ $p == "1" ] ; then val="9"$lga ; else val="B"$lga ; fi;;
* ) echo "Geographical addr must be below 0x20"; exit ;;
esac

./vmebur -sA16 -wD16 -q "m A000 2000;${num}8=${val}"


../cpldtool/cpldtool ${dnum} p main.bin
if [ $? == 0 ] ; then
    echo Initializing ...
    sleep 2
    ./progcsr.sh $ba
    ./pwdadc.sh $ba
    ./clockenb.sh $ba
    ./resetadc.sh $ba
    ./settrig.sh $ba
else
    ../cpldtool/cpldtool ${dnum} p
fi
