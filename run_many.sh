#!/bin/bash

for i in {1..3}
do
    trap "kill %$i" SIGINT
    cat input_examples.txt | (gawk -f ./random_sampling.awk -v n\=1000) | ./build/client | tee ./logs/$i.log | sed -e "s/^/[Client$i] /" &
done

./build/client | tee ./logs/11.log | sed -e "s/^/[Client11] /"

