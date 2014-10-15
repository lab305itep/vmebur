#!/bin/bash
#	Soft reset all ADCs
#
./vmebur -q "m 0 2000000;g 0 0=24;g 1 0=24;g 2 0=24;g 3 0=24;g 4 0=24;g 5 0=24;g 6 0=24;g 7 0=24;"
./vmebur -q "m 0 2000000;g 8 0=24;g 9 0=24;g A 0=24;g B 0=24;g C 0=24;g D 0=24;g E 0=24;g F 0=24;"
