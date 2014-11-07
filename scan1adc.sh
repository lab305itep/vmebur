#!/bin/bash
#
#       Known pattern types:
# 0 - all zeroes
# 1 - all ones
# 4 - Checkerboard (0xAAA / 0x555)
# 5 - PN23 ITU 0.150 X**23 + X**18 + 1
# 6 - PN9  ITU 0.150 X**9  + X**5 + 1
# 7 - One-/zero-word toggle (0xFFF / 0)
# 9 - 0xAAA
# 10 - 1×sync  0000 0111 1111

if [ "x"$2 == "x" ] ; then
    echo Usage: `basename $0` mode adc_unit
    exit
fi

case $1 in
0 ) reg14=0; reg0D=3;;
1 ) reg14=0; reg0D=2;;
4 ) reg14=0; reg0D=44;;
5 ) reg14=0; reg0D=5;;
6 ) reg14=0; reg0D=6;;
7 ) reg14=0; reg0D=47;;
9 ) reg14=0; reg0D=9;;
A ) reg14=0; reg0D=A;;
C ) reg14=0; reg0D=C;;
* ) echo "Unknown mode $1"; exit;;
esac

case $2 in
0 | 1 | 2 | 3 ) rbase=0;;
4 | 5 | 6 | 7 ) rbase=20;;
8 | 9 | a | b | A | B ) rbase=40;;
c | d | e | f | C | D | E | F ) rbase=60;;
* ) echo "Unknown unit $2"; exit;;
esac

./vmebur -q "m 0 2000000;g $2 14=$reg14;g $2 D=$reg0D;" >> /dev/null

for i in 0 1 2 3 4 5 6 7 8 9 A B ; do
    echo "================ $i ================="
    ./vmebur -q "m 0 2000000;g $2 16=$i"
    ./vmebur -q "m 0 2000000;w 100000;x ${rbase}03=30$1;x ${rbase}03=10$1;x ${rbase}03=$1;w 10000;x ${rbase}03=A$1;x ${rbase}03=0$1"
    ./vmebur -q "m 0 2000000;w 10000;x ${rbase}02"

    case $2 in
    0 | 4 | 8 | c | C ) ./vmebur -q "m 0 2000000;x ${rbase}18;x ${rbase}40;x ${rbase}41;x ${rbase}42;x ${rbase}43";;
    1 | 5 | 9 | d | D ) ./vmebur -q "m 0 2000000;x ${rbase}19;x ${rbase}44;x ${rbase}45;x ${rbase}46;x ${rbase}47";;
    2 | 6 | a | A | e | E ) ./vmebur -q "m 0 2000000;x ${rbase}1A;x ${rbase}48;x ${rbase}49;x ${rbase}4A;x ${rbase}4B";;
    3 | 7 | b | B | f | F ) ./vmebur -q "m 0 2000000;x ${rbase}1B;x ${rbase}4C;x ${rbase}4D;x ${rbase}4E;x ${rbase}4F";;
    esac
done
