#!/bin/sh

if [ $# -ne 6 ]; then
    echo "Usage: $0 graph size colors output threads seed"
    exit 1
fi

GRAPH="$1"
SIZE="$2"
COLORS="$3"
OUTPUT="$4"
THREADS="$5"
SEED="$6"

echo "[$(date)] Builing first table"
./motivo-build --graph "$GRAPH" --size 1 --colors "$COLORS" --output "$OUTPUT.1.cnt" --threads "$THREADS" --seed "$SEED" || exit 1
./motivo-merge --output "$OUTPUT.1" "$OUTPUT.1.cnt" || exit 1

for i in $(seq 2 "$SIZE"); do
    echo "[$(date)] Builing table of size $i"
    ./motivo-build --graph "$GRAPH" --size "$i" --tables-basename "$OUTPUT" --output "$OUTPUT.$i.cnt" --threads "$THREADS" || exit 1
    ./motivo-merge --output "$OUTPUT.$i" "$OUTPUT.$i.cnt" || exit 1
done

exit 0
