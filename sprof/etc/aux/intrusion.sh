#!/bin/bash

COUNT=0

while [ $COUNT -lt 100 ]
do
	echo Teste $(( $COUNT+1 ))
	(time ./blackscholes 64 in_10M.txt prices2.txt > /dev/null) &>> time_result.txt
    COUNT=$(( $COUNT+1 ))
done
