#!/usr/bin/env bash

#TODO Single graph
BUILDPATH=../build

TIME="$(which time)"
if [ "$TIME" == "" ]; then
    echo "Could not find 'time'"
    exit 1
else
TIME="$TIME --verbose"
fi

ADAPTIVE=YES
SMART=NO
COMPRESS_THRESHOLD=0
SELECTIVE_FILE=""
POSITIONAL=()
THREADS=0
while [[ $# -gt 0 ]]
do
    key="$1"
    case $key in
	-k)
	    SIZE="$2"
	    shift # past argument
	    shift # past value
	    ;;
	-g|--graph)
	    GRAPH="$2"
	    shift # past argument
	    shift # past value
	    ;;
	-s|--samples)
	    NSAMPLES="$2"
	    shift # past argument
	    shift # past value
	    ;;
	--smart)
	    SMART=YES
	    shift # past argument
	    ;;
	-o|--output)
	    OUTPUT=$2
	    shift # past argument
	    shift # past value
	    ;;
	--selective-treelets)
	    SELECTIVE_FILE=$2
	    shift # past argument
	    shift # past value
	    ;;
	-t|--threads)
	    THREADS=$2
	    shift
	    shift
	    ;;
	-c|--compress)
	    COMPRESS_THRESHOLD="$2"
	    shift # past argument
	    shift # past value
	    ;;
	-a|--adaptive)
	    ADAPTIVE=YES
	    shift # past argument
	    ;;
	*)    # unknown option
	    POSITIONAL+=("$1") # save it in an array for later
	    shift # past argument
	    ;;
    esac
done
set -- "${POSITIONAL[@]}" # restore positional parameters

print_usage() {
    echo "Usage: $0 (-g|--graph) GRAPH (-k) GRAPHLET_SIZE (-s|--samples) NUM_SAMPLES (-o|--output) OUTPUT [--smart] [-c compress_threshold] [-t selective_treelet_file]"
}

if [ -z ${GRAPH+x} ]; then echo "Missing input graph basename (-g,--graph)"; print_usage; exit 1; fi
if [ -z ${SIZE+x} ]; then echo "Missing graphlet size (-k)"; print_usage; exit 1; fi
if [ -z ${NSAMPLES+x} ]; then echo "Missing number of samples (-s,--samples)"; print_usage; exit 1; fi
if [ -z ${OUTPUT+x} ]; then echo "Missing output basename (-o,--output)"; print_usage; exit 1; fi

LOGFILE="$OUTPUT.log"
TIMEFILE="$OUTPUT.perf"

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
    echo $(grep -Eo "^(Building|Merge|Sampling) time: [0-9.]+ s$" "$1" | grep -Eo "[0-9.]+ s$" | cut -d' ' -f 1 | xargs printf "%.2f")
}

get_nthreads()
{
    echo $(grep -Eo "using [0-9]+ thread\(s\)$" "$1" | cut -d' ' -f 2)
    #echo 1
}

echo "[$(date)] MOTIVO Start" | tee $LOGFILE
echo "output,graph,size,colors,compress_threshold,type,ntreelets,nsamples,nthreads,walltime,usertime,systemtime,actualtime" > $TIMEFILE

echo -e "size\t\tbuild\t\tmerge\t\tsample"

# Smart: exclude stars from the building phase, and sample them separately in the sampling phase

EXTRA_BUILD_OPTS=()
if [ "$SELECTIVE_FILE" != "" ]; then
    EXTRA_BUILD_OPTS+=(--selective "$SELECTIVE_FILE")
fi

for i in $(seq 1 "$SIZE"); do
    
    if [ $i -eq "$SIZE" ]; then
        EXTRA_BUILD_OPTS+=(--store-on-0-colored-vertices-only)
	if [ "$SMART" == "YES" ]; then
	    echo "EXCLUDE" > exclude-star-$SIZE.txt
	    $BUILDPATH/motivo-decompose --star $SIZE --size $SIZE >> exclude-star-$SIZE.txt 2>/dev/null
	    SELECTIVE_FILE=exclude-star-$SIZE.txt
	fi
    fi

    echo -en "$i  \t\t"
    echo "[$(date)] Building table of size $i" >> $LOGFILE
    ($TIME $BUILDPATH/motivo-build --graph "$GRAPH" --size "$i" --colors "$SIZE" --tables-basename "$OUTPUT" --output "$OUTPUT" --threads "$THREADS" ${EXTRA_BUILD_OPTS[@]} > "$OUTPUT.b$i.log" 2>&1) || exit 1  
    echo -n $(get_walltime "$OUTPUT.b$i.log")
    echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,build,0,0,$(get_nthreads "$OUTPUT.b$i.log"),$(get_walltime "$OUTPUT.b$i.log"),$(get_usertime "$OUTPUT.b$i.log"),$(get_systemtime "$OUTPUT.b$i.log"),$(get_actualtime "$OUTPUT.b$i.log")" >> $TIMEFILE

    echo -en "\t\t"
    echo "[$(date)] Merging table of size $i" >> $LOGFILE
    ($TIME $BUILDPATH/motivo-merge --output "$OUTPUT.$i" --compress-threshold "$COMPRESS_THRESHOLD" "$OUTPUT.$i.cnt" > "$OUTPUT.m$i.log" 2>&1) || exit 1
    if [ $i -ne $SIZE ]; then
        echo $(get_walltime "$OUTPUT.m$i.log")
    else
        echo -n $(get_walltime "$OUTPUT.m$i.log")
    fi
    echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,merge,$(get_ntreelets "$OUTPUT.m$i.log"),0,0,$(get_walltime "$OUTPUT.m$i.log"),$(get_usertime "$OUTPUT.m$i.log"),$(get_systemtime "$OUTPUT.m$i.log"),$(get_actualtime "$OUTPUT.m$i.log")" >> $TIMEFILE

    echo "[$(date)] Done. Removing count file." >> $LOGFILE
    rm "$OUTPUT.$i.cnt"
done

# Smart: exclude stars from the building phase, and sample them separately in the sampling phase
if [ "$SMART" == "YES" ]; then
    echo "EXCLUDE" > exclude-star-$SIZE.txt
    $BUILDPATH/motivo-decompose --star $SIZE --size $SIZE >> exclude-star-$SIZE.txt 2>/dev/null
    SELECTIVE_FILE=exclude-star-$SIZE.txt
    EXTRA_SAMPLE_OPTS+=(--smart-stars)
elif [ "$ADAPTIVE" == "YES" ]; then
    EXTRA_SAMPLE_OPTS+=(--estimate-occurrences-adaptive)
fi

if [ "$SELECTIVE_FILE" != "" ]; then
    EXTRA_SAMPLE_OPTS+=(--selective "$SELECTIVE_FILE")
fi

echo -en "\t\t"
echo "[$(date)] Sampling..." >> $LOGFILE
($TIME $BUILDPATH/motivo-sample --graph "$GRAPH" --size "$SIZE" -n "$NSAMPLES" -i "$OUTPUT" -c --graphlets -o "$OUTPUT" --threads "$THREADS" ${EXTRA_SAMPLE_OPTS[@]} > "$OUTPUT.s$SIZE.log" 2>&1) || exit 1
echo $(get_walltime "$OUTPUT.s${SIZE}.log")
echo "$OUTPUT,$GRAPH,$i,$SIZE,$COMPRESS_THRESHOLD,sample,0,$NSAMPLES,$(get_nthreads "$OUTPUT.s$SIZE.log"),$(get_walltime "$OUTPUT.s$SIZE.log"),$(get_usertime "$OUTPUT.s$SIZE.log"),$(get_systemtime "$OUTPUT.s$SIZE.log"),$(get_actualtime "$OUTPUT.s$SIZE.log")" >> $TIMEFILE

echo "[$(date)] Done" | tee -a $LOGFILE

echo "Samples are in $OUTPUT.csv:"
head -6 $OUTPUT.csv

exit 0
