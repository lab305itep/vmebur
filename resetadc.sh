#!/bin/bash
#	Soft reset all ADCs
#
./vmebur -q "m 0 2000000;g 0 8=0;g 1 8=0;g 2 8=0;g 3 8=0;g 4 8=0;g 5 8=0;g 6 8=0;g 7 8=0;"
./vmebur -q "m 0 2000000;g 8 8=0;g 9 8=0;g A 8=0;g B 8=0;g C 8=0;g D 8=0;g E 8=0;g F 8=0;"
sleep 1
./vmebur -q "m 0 2000000;g 0 0=24;g 1 0=24;g 2 0=24;g 3 0=24;g 4 0=24;g 5 0=24;g 6 0=24;g 7 0=24;"
./vmebur -q "m 0 2000000;g 8 0=24;g 9 0=24;g A 0=24;g B 0=24;g C 0=24;g D 0=24;g E 0=24;g F 0=24;"
./vmebur -q "m 0 2000000;g 0 14=0;g 1 14=0;g 2 14=0;g 3 14=0;g 4 14=0;g 5 14=0;g 6 14=0;g 7 14=0;"
./vmebur -q "m 0 2000000;g 8 14=0;g 9 14=0;g A 14=0;g B 14=0;g C 14=0;g D 14=0;g E 14=0;g F 14=0;"
./vmebur -q "m 0 2000000;g 0 D=5;g 1 D=5;g 2 D=5;g 3 D=5;g 4 D=5;g 5 D=5;g 6 D=5;g 7 D=5;"
./vmebur -q "m 0 2000000;g 8 D=5;g 9 D=5;g A D=5;g B D=5;g C D=5;g D D=5;g E D=5;g F D=5;"
#	Set DAC to midrange
./vmebur -q "m 0 2000000;s 2000"
#	Reset receivers
./vmebur -q "m 0 2000000;x 3=305;x 2003=305;x 4003=305;x 6003=305;x 3=105;x 2003=105;x 4003=105;x 6003=105"
./vmebur -q "m 0 2000000;x 3=5;x 2003=5;x 4003=5;x 6003=5;x 3=D5;x 2003=D5;x 4003=D5;x 6003=D5"
./vmebur -q "m 0 2000000;x 3=55;x 2003=55;x 4003=55;x 6003=55"
sleep 1
#	Check Ready and bitslips
echo Check ready - should see D5
./vmebur -q "m 0 2000000;x 2;x 2002;x 4002;x 6002"
echo Check Frame
./vmebur -q "m 0 2000000;x 18;x 19;x 1A;x 1B;x 2018;x 2019;x 201A;x 201B"
./vmebur -q "m 0 2000000;x 4018;x 4019;x 401A;x 401B;x 6018;x 6019;x 601A;x 601B"
echo Check Data
for i in 0 20 40 60 ; do
    ./vmebur -q "m 0 2000000;x ${i}40;x ${i}41;x ${i}42;x ${i}43;x ${i}44;x ${i}45;x ${i}46;x ${i}47"
    ./vmebur -q "m 0 2000000;x ${i}48;x ${i}49;x ${i}4A;x ${i}4B;x ${i}4C;x ${i}4D;x ${i}4E;x ${i}4F"
done 
./vmebur -q "m 0 2000000;g 0 D=0;g 1 D=0;g 2 D=0;g 3 D=0;g 4 D=0;g 5 D=0;g 6 D=0;g 7 D=0;"
./vmebur -q "m 0 2000000;g 8 D=0;g 9 D=0;g A D=0;g B D=0;g C D=0;g D D=0;g E D=0;g F D=0;"
