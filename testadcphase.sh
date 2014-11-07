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

if [ $2"x" == "x" ] ; then
    echo "Usage testadcphas.sh mode unit"
    echo "Known modes: 0, 1, 4, 5, 6, 7, 9, A"
    echo "Units: 0 - F"
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
0 | 1 | 2 | 3 ) csr=3;    err=0;;
4 | 5 | 6 | 7 ) csr=2003; err=20;;
8 | 9 | A | B ) csr=4003; err=40;;
C | D | E | F ) csr=6003; err=60;;
esac

if [ $2"x" == "x" ] ; then
    echo "Usage testadcphas.sh mode unit"
    echo "Known modes: 0, 1, 4, 5, 6, 7, 9, A, C"
    echo "Units: 0 - F"
    exit
fi

./vmebur -q "m 0 2000000;g $2 14=$reg14;g $2 D=$reg0D;" >> /dev/null

# reset IODELAY
./vmebur -q "m 0 2000000;x $csr=40$1;w 1;x $csr=20$1;x $csr=$1" >> /dev/null

# increment and check
for (( i=0; $i<256; i=$i+1 )) ; do
# increment phase & start
    ./vmebur -q "m 0 2000000;x $csr=10$1;x $csr=$1;w 1;x $csr=A$1;x $csr=$1" >> /dev/null
#    ./vmebur -q "m 0 2000000;x $csr=105;x $csr=5;w 1;x $csr=A5;x $csr=5" >> /dev/null
# print result
    echo "=============== $i ======================"
    case $2 in
    0 | 4 | 8 | C ) ./vmebur -q "m 0 2000000;x ${err}18;x ${err}40;x ${err}41;x ${err}42;x ${err}43";;
    1 | 5 | 9 | D ) ./vmebur -q "m 0 2000000;x ${err}19;x ${err}44;x ${err}45;x ${err}46;x ${err}47";;
    2 | 6 | A | E ) ./vmebur -q "m 0 2000000;x ${err}1A;x ${err}48;x ${err}49;x ${err}4A;x ${err}4B";;
    3 | 7 | B | F ) ./vmebur -q "m 0 2000000;x ${err}1B;x ${err}4C;x ${err}4D;x ${err}4E;x ${err}4F";;
    esac
done

# reset IODELAY
./vmebur -q "m 0 2000000;x $csr=40$1;w 1;x $csr=20$1;x $csr=$1" >> /dev/null
