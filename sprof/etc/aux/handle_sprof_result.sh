#!/bin/bash

echo Blackscholes > sprof_analysis_blackscholes.txt
echo >> sprof_analysis_blackscholes.txt

cat ./blackscholes.txt | grep -A 6 '64 threads' | grep 'Total time' | cut -c 29-37 > thr_results.txt

echo -e "\nMean" >> sprof_analysis_blackscholes.txt
COUNT=3
MEANCALC=$(echo $(sed "1q;d" thr_results.txt) + $(sed "2q;d" thr_results.txt) | bc)

while [ $COUNT -le 100 ]
do
	A=$COUNT
	MEANCALC=$(echo $MEANCALC + $(sed "${A}q;d" thr_results.txt) | bc)
	COUNT=$(( $COUNT + 1 ))
done

MEAN=$(echo $MEANCALC / 100 | bc -l | awk '{printf "%.6f", $1}') 
echo $MEAN >> sprof_analysis_blackscholes.txt

echo -e "\nStandard deviation" >> sprof_analysis_blackscholes.txt
COUNT=2
DEVCALC=$(echo $(sed "1q;d" thr_results.txt) - $MEAN | bc)
DEVCALC=$(echo $DEVCALC^2 | bc)
while [ $COUNT -le 100 ]
do
	A=$COUNT
	DEVCALC2=$(echo $(sed "${A}q;d" thr_results.txt) - $MEAN | bc)
	DEVCALC2=$(echo $DEVCALC2^2 | bc)
	DEVCALC=$(echo $DEVCALC + $DEVCALC2 | bc)
	COUNT=$(( $COUNT + 1 ))
done

DEV=$(echo "sqrt($DEVCALC / 100)" | bc -l | awk '{printf "%.6f", $1}')
echo $DEV >> sprof_analysis_blackscholes.txt

