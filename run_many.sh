#!/bin/bash

for i in {1..2}
do
    trap "kill %$i" SIGINT
    #cat input_examples.txt | (gawk -f ./random_sampling.awk -v n\=1000) | ./build/client | tee ./logs/$i.log | sed -e "s/^/[Client$i] /" &
    while read num; do
        echo "insert $num $num"
    done < <(seq $((1 + 100000*$i)) $(( 100000 + 100000*$i))) | ./build/client | tee ./logs/$i.log | sed -e "s/^/[Client$i] /" &
done

./build/client | tee ./logs/11.log | sed -e "s/^/[Client11] /"

