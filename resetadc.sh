#!/bin/bash
#	Soft reset all ADCs
#
./vmebur -q "m 0 2000000;g 0 0=24;g 1 0=24;g 2 0=24;g 3 0=24;g 4 0=24;g 5 0=24;g 6 0=24;g 7 0=24;"
./vmebur -q "m 0 2000000;g 8 0=24;g 9 0=24;g A 0=24;g B 0=24;g C 0=24;g D 0=24;g E 0=24;g F 0=24;"
./vmebur -q "m 0 2000000;g 0 14=0;g 1 14=0;g 2 14=0;g 3 14=0;g 4 14=0;g 5 14=0;g 6 14=0;g 7 14=0;"
./vmebur -q "m 0 2000000;g 8 14=0;g 9 14=0;g A 14=0;g B 14=0;g C 14=0;g D 14=0;g E 14=0;g F 14=0;"
#	Set DAC to midrange
./vmebur -q "m 0 2000000;s 2000"
