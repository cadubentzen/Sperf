#!/bin/bash

cd /home/marcio/sprof

COUNT=0
while [ $COUNT -lt 100 ]
do
	echo >> ./etc/time_result1.txt
	echo Teste bodytrack $(( $COUNT+1 )) >> ./etc/time_result1.txt
	(time ./bodytrack ~/sequenceB_261 4 20 6000 12 3 32 1 > /dev/null) &>> ./etc/time_result1.txt
    COUNT=$(( $COUNT+1 ))
done

COUNT=0
while [ $COUNT -lt 100 ]
do
	echo >> ./etc/time_result2.txt
	echo Teste freqmine $(( $COUNT+1 )) >> ./etc/time_result2.txt
	export OMP_NUM_THREADS=32
	(time ./freqmine ~/webdocs_250k.dat 14000 > /dev/null) &>> ./etc/time_result2.txt
    COUNT=$(( $COUNT+1 ))
done



