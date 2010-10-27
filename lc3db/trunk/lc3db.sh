#!/bin/bash
# wrapper for lc3db

declare -a ARGS
i=0
for f in "$@" 
do
	if [ "$f" = "-fullname" ]
       	then
	       	ARGS[$i]="-f"
	else
		ARGS[$i]="$f"
	fi
	i=$((i+1))
done

echo wrapper: "$0".bin  ${ARGS[@]}
"$0".bin  ${ARGS[@]}
