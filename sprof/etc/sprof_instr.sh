#!/bin/bash
#
# Script file to execute the automatic instrumentation of source code
#

# Copyright (C) 2017 Márcio Jales
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'
EXT_COUNT=1
MARK_COUNT=1
INVALID_LINE=0

cleaning()
{
	# Retrieve how many "# pragmas omp parallel" exist in the file
	NUM_PRAGMAS=$(grep '^\s*#' $FPARSE | grep 'pragma omp parallel' | wc -l)

	if [[ $NUM_PRAGMAS -gt 0 ]]
	then
		echo -e "${BLUE}[sprof_instr.sh]${NC} Cleaning $FPARSE"
		sed -i -e 's/sprof_start();//g' $FPARSE
		sed -i -e 's/sprof_stop();//g' $FPARSE
		sed -i '/^[^/].*sprofops.h"$/d' $FPARSE
	else
		echo -e "${YELLOW}[sprof_instr.sh]${NC} Nothing to cleaning in $FPARSE"
	fi
}

is_valid()
{
	RANGE_COUNT=1

	if [[ -a $CONF_PATH/comm_temp ]]
	then
		while [[ $RANGE_COUNT -le $(cat $CONF_PATH/comm_temp | wc -l) ]]
		do
			RES1=$(sed -n "$RANGE_COUNT p" $CONF_PATH/comm_temp | cut -f 1 -d'-')
			RES2=$(sed -n "$RANGE_COUNT p" $CONF_PATH/comm_temp | cut -f 2 -d'-')
			if [[ $LINE -ge $RES1  ]] && [[ $LINE -le $RES2  ]]
			then
				INVALID_LINE=1
				break
			fi
			RANGE_COUNT=$(( $RANGE_COUNT+1 ))
		done
	fi
}

set_start()
{
	case "$1" in
		openmp)

		LINE=$(grep -n '^\s*#' $FPARSE | grep 'pragma omp parallel' | cut -f 1 -d':' | sed -n "$PRAGMA_COUNT p")
		is_valid
		
		if [[ $INVALID_LINE -eq 0 ]]
		then
			sed -i "$LINE i sprof_start();" $FPARSE
		fi
		;;

		pthreads)

		echo
		;;
	esac
}

set_stop()
{
	case "$1" in
		openmp)
	
		if [[ $INVALID_LINE -eq 0  ]]
		then
			(sed -n "$LINE,\$p" $FPARSE | grep -n {) > $CONF_PATH/temp1		# o '\' é necessário para escapar o $, pois está entre aspas duplas
			(sed -n "$LINE,\$p" $FPARSE | grep -n }) > $CONF_PATH/temp2
			cat $CONF_PATH/temp2 >> $CONF_PATH/temp1
			sort -n $CONF_PATH/temp1 > $CONF_PATH/temp
			rm $CONF_PATH/temp1 $CONF_PATH/temp2
			
			BRACKET_COUNT=1
			ITER=2
	
			while true
			do
				if [[ -z $(sed -n "$ITER p" $CONF_PATH/temp | grep }) ]]
				then
					BRACKET_COUNT=$(( $BRACKET_COUNT+1 ))
					ITER=$(( $ITER+1 ))
				else
					BRACKET_COUNT=$(( $BRACKET_COUNT-1 ))
					if [[ $(sed -n "$ITER p" $CONF_PATH/temp | grep } | grep {) ]]
					then
						BRACKET_COUNT=$(( $BRACKET_COUNT+1 ))
					fi

					if [[ $BRACKET_COUNT -eq 0 ]]
					then
						LINE=$(( $LINE + $(sed -n "$ITER p" $CONF_PATH/temp | cut -f 1 -d':') - 1 ))
						sed -i "$LINE a sprof_stop();" $FPARSE
						break
					fi

					ITER=$(( $ITER+1 ))
				fi		
			done	

			LOCAL_MARK_COUNT=$(( $LOCAL_MARK_COUNT+1 ))					
			MARK_COUNT=$(( $MARK_COUNT+1 ))
			rm $CONF_PATH/temp
		fi

		PRAGMA_COUNT=$(( $PRAGMA_COUNT+1 ))
		;;
	
		pthreads)
		
		echo
		;;
	esac
}

id_comments()
{
	grep -n -e "^\s*/[*]" -e "[*]/$" $FPARSE > $CONF_PATH/temp
	(grep "[*]/" $CONF_PATH/temp | grep "/[*]") > $CONF_PATH/temp2

	ITER1=1
	ITER2=1

	while true
	do
		if [[ $ITER1 -gt $(cat $CONF_PATH/temp | wc -l) ]]
		then
			break
		fi

		if [[ $(sed -n "$ITER1 p" $CONF_PATH/temp | cut -f 1 -d':') -eq $(sed -n "$ITER2 p" $CONF_PATH/temp2 | cut -f 1 -d':') ]]
		then
			sed -i "$ITER1 d" $CONF_PATH/temp 
			ITER2=$(( $ITER2+1 ))
			continue
		fi
		ITER1=$(( $ITER1+1 ))
	done
	
	ITER1=1
	while [[ $ITER1 -lt $(cat $CONF_PATH/temp | wc -l) ]]
	do
		echo "$(sed -n "$ITER1 p" $CONF_PATH/temp | cut -f 1 -d':')-$(sed -n "$(( $ITER1+1 )) p" $CONF_PATH/temp | cut -f 1 -d':')" >> $CONF_PATH/comm_temp
		ITER1=$(( $ITER1+2 ))
	done
	
	rm $CONF_PATH/temp $CONF_PATH/temp2
}


echo -e "${BLUE}[sprof_instr.sh]${NC} Retriving the application's source code path"
# Path to sprof_instr.sh
EXEC_PATH=$(dirname $0)
cd $EXEC_PATH
CONF_PATH=$PWD
# Retrieving Retriving the application's source code path from sprof_instr.conf
TARGET_PATH=$(perl -ne 'print if /\b^DPATH\b/' sprof_instr.conf | cut -f 2 -d'=')
# Path to sprofops.h
INCLUDE_PATH=../include
cd $INCLUDE_PATH
INCLUDE_PATH=$PWD
cd ../etc
# Get the file extentions that will be instrumented
EXT=$(perl -ne 'print if /\b^EXTENSIONS\b/' sprof_instr.conf | cut -f 2 -d'=' | cut -f $EXT_COUNT -d',')

if [[ ! -d $TARGET_PATH  ]]
then
	echo -e "${RED}[sprof_instr.sh]${NC} No such folder path"
	exit
fi

echo -e "${BLUE}[sprof_instr.sh]${NC} Entering the folder"
cd $TARGET_PATH

echo -e "${BLUE}[sprof_instr.sh]${NC} Parsing the files..."

# While there is extentions that correpond the files to parse
while [[ ! -z $EXT ]]
do
	# Get the number of files with a specific extention
	NUM_FILES=$(find . -name "*$EXT" | wc -l)
	FILE_COUNT=1

	# Iterate through the files
	while [[ $FILE_COUNT -le $NUM_FILES ]]
	do
		# Get the nth file of the current extention
		FPARSE=$(find . -name "*$EXT" | sed -n "$FILE_COUNT p")
		echo -e "${BLUE}[sprof_instr.sh]${NC} $FPARSE"

		# If the files will be compiled using openmp API
		case "$1" in 
			openmp)
			# Retrieve how many "# pragmas omp parallel" exist in the file
			NUM_PRAGMAS=$(grep '^\s*#' $FPARSE | grep 'pragma omp parallel' | wc -l)

			# If there is any "pragma omp parallel"
			if [[ $NUM_PRAGMAS -gt 0 ]]
			then
				LOCAL_MARK_COUNT=1
				PRAGMA_COUNT=1
				sed -i '1 i #include "'$INCLUDE_PATH'/sprofops.h"' $FPARSE
				
				while [[ $PRAGMA_COUNT -le $NUM_PRAGMAS ]]
				do
					id_comments

					set_start $1
					set_stop $1
					
					if [[ -a $CONF_PATH/comm_temp ]]
					then
						rm $CONF_PATH/comm_temp
					fi

					INVALID_LINE=0
				done
				echo -e "${BLUE}[sprof_instr.sh]${NC} $(( $LOCAL_MARK_COUNT-1 )) marks on $FPARSE file"
				
			else
				echo -e "${YELLOW}[sprof_instr.sh]${NC} Nothing to parse on $FPARSE"
			fi
			;;
		# If the files will be compiled using pthreads
			pthreads)
			
			echo "Parse missing"
			;;

			clean)
	
			cleaning			
			;;

			*)

			echo -e "${RED}[sprof_instr.sh]${NC} Invalid argument. Usage: <path/to/sprof_instr.sh> openmp|pthreads|clean"
			exit
			;;
		esac

		FILE_COUNT=$(( $FILE_COUNT+1 ))
	done
	
	EXT_COUNT=$(( $EXT_COUNT+1 ))
	EXT=$(perl -ne 'print if /\b^EXTENSIONS\b/' $CONF_PATH/sprof_instr.conf | cut -f 2 -d'=' | cut -f $EXT_COUNT -d',')
done

case "$1" in
	openmp|pthreads)
	
	echo -e "${BLUE}[sprof_instr.sh]${NC} Total of $(( $MARK_COUNT-1 )) marks done"
	;;
esac






