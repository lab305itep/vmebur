#!/bin/bash

if [ "x"$1 != "x" ] ; then
	echo "=====     Init and get self-trigger from all 60 channels      ====="
	echo "Usage ./doall.sh [?]"
	echo "Parameters are taken from Parameters.sh"
	exit
fi

. Parameters.sh

./init.sh $UNIT_NUMBER $UNIT_GA $ADC_PHASE

for i in $RUN_LIST ; do
	./getdata.sh $UNIT_GA $i $THRESHOLD $RUN_TIME
done
