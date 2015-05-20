ba=$1
if [ $ba"x" == "x" ] ; then ba="0" ; fi 
#	Reset WB @ channel FPGAs
./vmebur -q "m $ba 2000000;10004=80000000;10004=0"
sleep 1
#	setup I2C clock
./vmebur -q "m $ba 2000000;20000=C0000000;20004=0;20008=80000000"
sleep 1
#	setup I2C clock, 16-chan #0-3
./vmebur -q "m $ba 2000000;x 10=C000;x 11=0;x 12=8000;x 2010=C000;x 2011=0;x 2012=8000;x 4010=c000;x 4011=0;x 4012=8000;x 6010=c000;x 6011=0;x 6012=8000"
