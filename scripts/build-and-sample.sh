#!/usr/bin/env bash

#TODO Single graph

TIME="$(which time)"
if [ "$TIME" == "" ]; then
    echo "Could not find 'time'"
    exit 1
else
TIME="$TIME --verbose"
fi

if [ $# != 4 -a $# != 5 -a $# != 6 ]; then
    echo "Usage: $0 graph size nsamples output [compress_threshold] [selective_file]"
    exit 1
fi

GRAPH="$1"
SIZE="$2"
NSAMPLES="$3"
OUTPUT="$4"

COMPRESS_THRESHOLD="0"
if [ "$5" != "" ]; then
    COMPRESS_THRESHOLD="$5"
fi

SELECTIVE_FILE=""
if [ "$6" != "" ]; then
    SELECTIVE_FILE="$6"
fi

get_walltime()
{
        elapsed=$(grep "Elapsed (wall clock) time" "$1" | grep -Eo "[^ ]+$")
        a=$(echo "$elapsed" | cut -d ":" -f 1)
        b=$(echo "$elapsed" | cut -d ":" -f 2)
        c=$(echo "$elapsed" | cut -d ":" -f 3)

        if [ -z "$c" ]; then #time is in mm:ss format
                echo $(echo "$a*60 + $b" | bc)
        else #time is in hh:mm:ss format
                echo $(echo "$a*3600 + $b*60 + $c" | bc )
        fi
}

get_usertime()
{
    echo $(grep "User time (seconds):" "$1" | grep -Eo "[^ ]+$")
}

get_systemtime()
{
    echo $(grep "System time (seconds):" "$1" | grep -Eo "[^ ]+$")
}

get_ntreelets()
{
    echo $(grep -Eo "^Total number of treelet occurrences: [0-9]+" "$1" | grep -Eo "[0-9]+$")
}

get_actualtime()
{
    echo $(grep -Eo "^(Building|Merge|Sampling) time: [0-9.]+ s$" "$1" | grep -Eo "[0-9.]+ s$" | cut -d' ' -f 1)
}

get_nthreads()
{
    echo $(grep -Eo "using [0-9]+ thread\(s\)$" "$1" | cut -d' ' -f 2)
}

echo "##Run at $(date)" >&2
echo "#output,graph,size,colors,compress_threshold,type,ntreelets,nsamples,nthreads,walltime,usertime,systemtime,actualtime" >&2

for i in $(seq 1 "$SIZE"); do

    EXTRA_BUILD_OPTS=()
    if [ "$SELECTIVE_FILE" != "" ]; then
        EXTRA_BUILD_OPTS=(--selective "$SELECTIVE_FILE")
    else
        if [ $i -eq "$SIZE" ]; then
            EXTRA_BUILD_OPTS=(--store-on-0-colored-vertices-only)
        fi
    fi

    echo "[$(date)] Building table of size $i"
    ($TIME ./motivo-build --graph "$GRAPH" --size "$i" --colors "$SIZE" --tables-basename "$OUTPUT" --output "$OUTPUT" --threads 0 ${EXTRA_BUILD_OPTS[@]} > >(tee "$OUTPUT.b$i.log") 2>&1 ) || exit 1
    echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,build,0,0,$(get_nthreads "$OUTPUT.b$i.log"),$(get_walltime "$OUTPUT.b$i.log"),$(get_usertime "$OUTPUT.b$i.log"),$(get_systemtime "$OUTPUT.b$i.log"),$(get_actualtime "$OUTPUT.b$i.log")" >&2

    echo "[$(date)] Merging table of size $i"
    ($TIME ./motivo-merge --output "$OUTPUT.$i" --compress-threshold "$COMPRESS_THRESHOLD" "$OUTPUT.$i.cnt" > >(tee "$OUTPUT.m$i.log") 2>&1) || exit 1
    echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,merge,$(get_ntreelets "$OUTPUT.m$i.log"),0,0,$(get_walltime "$OUTPUT.m$i.log"),$(get_usertime "$OUTPUT.m$i.log"),$(get_systemtime "$OUTPUT.m$i.log"),$(get_actualtime "$OUTPUT.m$i.log")" >&2

    echo "[$(date)] Done. Removing count file."
    rm "$OUTPUT.$i.cnt"
done

EXTRA_SAMPLE_OPTS=()
if [ "$SELECTIVE_FILE" == "" ]; then
    EXTRA_SAMPLE_OPTS=(--spanning-trees-no)
fi

echo "[$(date)] Sampling..."
($TIME ./motivo-sample --graph "$GRAPH" --size "$SIZE" -n "$NSAMPLES" -i "$OUTPUT" -t -c --graphlets -o "$OUTPUT" --footprints --no-rejection --group --threads 0 ${EXTRA_SAMPLE_OPTS[@]} > >(tee "$OUTPUT.s$SIZE.log") 2>&1) || exit 1
echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,sample,0,$NSAMPLES,$(get_nthreads "$OUTPUT.s$SIZE.log"),$(get_walltime "$OUTPUT.s$SIZE.log"),$(get_usertime "$OUTPUT.s$SIZE.log"),$(get_systemtime "$OUTPUT.s$SIZE.log"),$(get_actualtime "$OUTPUT.s$SIZE.log")" >&2

echo "[$(date)] Done"

exit 0
