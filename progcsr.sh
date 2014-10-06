#	Setup CSR space
./vmebur -s26 -q "m 80000 80000;7ff6c=24;7fff8=10;p 0 100;p 7ff00 100"
#	setup I2C
./vmebur -q "m 0 2000000;20000=C0000000;20004=0;20008=80000000"
