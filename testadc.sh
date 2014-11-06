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

for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F ; do
    ./vmebur -q "m 0 2000000;g $i 14=$reg14;g $i D=$reg0D;" >> /dev/null
done

for i in 3 2003 4003 6003 ; do
    ./vmebur -q "m 0 2000000;x $i=40$1;w 1;x $i=20$1;x $i=$1;x $i=8$1;x $i=$1" >> /dev/null
done

./vmebur -q "m 0 2000000;x 2;x 2002;x 4002;x 6002"

for i in 4 204 404 604 ; do
    ./vmebur -q "m 0 2000000;x ${i}0;x ${i}1;x ${i}2;x ${i}3;x ${i}4;x ${i}8;x ${i}C"
done
