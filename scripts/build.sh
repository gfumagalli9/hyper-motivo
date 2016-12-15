#!/bin/bash

if [[ $# -ne 4 ]] ; then
    echo "Usage: $0 graph size colors output"
    exit 1
fi

./motivo-build -g $1 -s 1 -c $3 -o $4.1
./motivo-merge -o $4.1 $4.1.cnt
for i in $(seq 2 $2); do
    ./motivo-build -g $1 -s $i -t $1 -o $4.$i
    ./motivo-merge -o $4.$i $4.$i.cnt
done