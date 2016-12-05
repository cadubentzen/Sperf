#!/bin/bash

echo Blackscholes > time_analysis_blackscholes.txt
echo >> time_analysis_blackscholes.txt

COUNT=1	
while [ $COUNT -le 100 ]
do
	AUX1=$(echo "$(cat time_result3.txt | grep real | awk '{print $2}' | sed "${COUNT}q;d" | cut -f 1 -d 'm') * 60")
	AUX2=$(echo "$(cat time_result3.txt | grep real | awk '{print $2}' | sed "${COUNT}q;d" | cut -f 1 -d 's' | cut -f 2 -d 'm')")
	echo "$AUX1 + $AUX2" | bc >> aux.txt
	COUNT=$(( $COUNT + 1 ))
done

echo -e "\nMean" >> time_analysis_blackscholes.txt
COUNT=3
MEANCALC=$(echo $(sed "1q;d" aux.txt) + $(sed "2q;d" aux.txt) | bc)

while [ $COUNT -le 100 ]
do
	A=$COUNT
	MEANCALC=$(echo $MEANCALC + $(sed "${A}q;d" aux.txt) | bc)
	COUNT=$(( $COUNT + 1 ))
done

MEAN=$(echo $MEANCALC / 100 | bc -l | awk '{printf "%.6f", $1}') 
echo $MEAN >> time_analysis_blackscholes.txt

echo -e "\nStandard deviation" >> time_analysis_blackscholes.txt
COUNT=2
DEVCALC=$(echo $(sed "1q;d" aux.txt) - $MEAN | bc)
DEVCALC=$(echo $DEVCALC^2 | bc)
while [ $COUNT -le 100 ]
do
	A=$COUNT
	DEVCALC2=$(echo $(sed "${A}q;d" aux.txt) - $MEAN | bc)
	DEVCALC2=$(echo $DEVCALC2^2 | bc)
	DEVCALC=$(echo $DEVCALC + $DEVCALC2 | bc)
	COUNT=$(( $COUNT + 1 ))
done

DEV=$(echo "sqrt($DEVCALC / 100)" | bc -l | awk '{printf "%.6f", $1}')
echo $DEV >> time_analysis_blackscholes.txt

