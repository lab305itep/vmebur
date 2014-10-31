#	Set CSR address and release reset
./vmebur -s0 -ms "m A000 2000;18=81"
#	Setup CSR space
./vmebur -s26 -q "m 80000 80000;7ff6c=24;7fff8=10"
./vmebur -s26 -q "m 80000 80000;p 0 100;p 7ff00 100"
#	Reset WB @ channel FPGAs
./vmebur -q "m 0 2000000;10004=80000000;10004=0"
sleep 1
#	setup I2C clock
./vmebur -q "m 0 2000000;20000=C0000000;20004=0;20008=80000000"
sleep 1
#	setup I2C clock, 16-chan #0-3
./vmebur -q "m 0 2000000;x 10=C000;x 11=0;x 12=8000;x 2010=C000;x 2011=0;x 2012=8000;\
x 4010=c000;x 4011=0;x 4012=8000;x 6010=c000;x 6011=0;x 6012=8000"
