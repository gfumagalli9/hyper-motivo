#!/usr/bin/env bash
set -euo pipefail

# --- CONFIGURAZIONE DI BASE ---
BUILDPATH=../build/bin

# Trova un comando 'time' GNU-compatibile
if command -v gtime &>/dev/null; then
    TIMECMD="gtime --verbose"
elif time --version &>/dev/null; then
    TIMECMD="time --verbose"
else
    TIMECMD="time"
fi

# --- helper: estrai solo i secondi da "h:mm:ss" o "m:ss" ---
format_time(){
  local raw=$1
  local sec="${raw##*:}"
  sec="${sec#0}"
  [[ -z "$sec" ]] && sec="0"
  echo "$sec"
}

usage(){
  cat <<EOF
Usage: $0 -g GRAPH -k MAXSIZE -o OUTPUT [-t THREADS] [-c COMP_THR] [-T THRESHOLD] [--seed SEED]

  -g|--graph            basename dell'iper-grafo (senza estensione)
  -k|--maxsize          massima k (>=2)
  -o|--output           prefisso per i file di output
  -t|--threads THREADS  numero di thread (default 1)
  -c|--comp-thr COMP    soglia compressione (default 0)
  -T|--threshold THR    soglia per hgsplit (default 0)
  --seed SEED           seme per RNG (opzionale)
EOF
  exit 1
}

# --- PARSING ARGOMENTI ---
THREADS=1
COMP_THR=0
THRESHOLD=0
SEED=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -g|--graph)     GRAPH=$2;       shift 2 ;;
    -k|--maxsize)   MAXSIZE=$2;     shift 2 ;;
    -o|--output)    OUTPUT=$2;      shift 2 ;;
    -t|--threads)   THREADS=$2;     shift 2 ;;
    -c|--comp-thr)  COMP_THR=$2;    shift 2 ;;
    -T|--threshold) THRESHOLD=$2;   shift 2 ;;
    --seed)         SEED=$2;        shift 2 ;;
    -h|--help)      usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

: "${GRAPH:?Missing -g/--graph}"
: "${MAXSIZE:?Missing -k/--maxsize}"
: "${OUTPUT:?Missing -o/--output}"

LOGFILE="$OUTPUT.log"
mkdir -p "$(dirname "$LOGFILE")"
echo "[$(date)] Start combined workflow" | tee "$LOGFILE"

# -------------------------------------------------------------------------
# 1) k=1 build + merge (base coloration) e duplicazione .dtz
# -------------------------------------------------------------------------
# 1.1) build k=1
printf "build1\t\t"
BUILD_RAW=$(
  $TIMECMD $BUILDPATH/motivo-build \
    --hyper --graph "$GRAPH" --size 1 \
    --colors "$MAXSIZE" --tables-basename "$OUTPUT" \
    --output "$OUTPUT" --threads "$THREADS" \
    ${SEED:+--seed "$SEED"} \
  2>&1 | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$BUILD_RAW")"

# 1.2) merge k=1
printf "merge1\t\t"
MERGE1_RAW=$(
  $TIMECMD $BUILDPATH/motivo-merge \
    --output "${OUTPUT}.1" --compress-threshold "$COMP_THR" \
    "${OUTPUT}.1.cnt" \
  2>&1 | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$MERGE1_RAW")"

# 1.3) duplichiamo le tabelle 1.dtz in Low e High
cp "${OUTPUT}.1.dtz"     "${OUTPUT}-Low.1.dtz"
cp "${OUTPUT}.1.dtz"     "${OUTPUT}-High.1.dtz"
cp "${OUTPUT}.1.treelets.dtz" "${OUTPUT}-High.1.treelets.dtz"

# -------------------------------------------------------------------------
# 2) split ipergrafo in Low / High
# -------------------------------------------------------------------------
printf "hgsplit\t\t"
SPLIT_RAW=$(
  $TIMECMD $BUILDPATH/motivo-hgsplit \
    -i "$GRAPH" \
    -s "${OUTPUT}-Low" \
    -l "${OUTPUT}-High" \
    -t "$THRESHOLD" \
  2>&1 | tee "$OUTPUT.split.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$SPLIT_RAW")"

# -------------------------------------------------------------------------
# 3) Gaifman sulla parte Low
# -------------------------------------------------------------------------
printf "gaifman\t\t"
GAIF_RAW=$(
  $TIMECMD $BUILDPATH/motivo-gaifman \
    -i "${OUTPUT}-Low" \
    -o "${OUTPUT}-Low-Gaifman" \
  2>&1 | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$GAIF_RAW")"

# -------------------------------------------------------------------------
# 4) nws + merge-ie per parte High k=1
# -------------------------------------------------------------------------
printf "high-nws\t"
NWS1_RAW=$(
  $TIMECMD $BUILDPATH/motivo-nws \
    --graph "${OUTPUT}-High" --size 1 \
    -i "${OUTPUT}-High" --output "${OUTPUT}-High" --threads "$THREADS"\
  2>&1 | tee "$OUTPUT.nwsH1.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$NWS1_RAW")"

printf "high-merge2\t"
MERGE2_1_RAW=$(
  $TIMECMD $BUILDPATH/motivo-merge \
    -e --output "${OUTPUT}-High.1.ie" --compress-threshold "$COMP_THR" \
    "${OUTPUT}-High.1.ie.cnt" \
  2>&1 | tee "$OUTPUT.nwsMerge.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
)
printf "%s\n" "$(format_time "$MERGE2_1_RAW")"

# rimuovo i file temporanei di k=1
# rm -f "${OUTPUT}-High.1.cnt" "${OUTPUT}-High.1.ie.cnt" "${OUTPUT}.1.cnt"   "${OUTPUT}.1.dtz" "${OUTPUT}.1.treelets.dtz"

# -------------------------------------------------------------------------
# 5) workflow classico su Low-Gaifman (k=2..MAXSIZE)
# -------------------------------------------------------------------------
echo -e "step\tk\tbuild\tmerge"  # intestazione per la sezione low
for ((k=2; k<=MAXSIZE; k++)); do
  # 5.1) build low
  LOW_B_RAW=$(
    $TIMECMD $BUILDPATH/motivo-build \
      --graph "${OUTPUT}-Low-Gaifman" --size "$k" \
      --tables-basename "${OUTPUT}-Low" \
      --output "${OUTPUT}-Low" --threads "$THREADS" \
      2>&1 | tee "$OUTPUT.buildL${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  LOW_B=$(format_time "$LOW_B_RAW")

  # 5.2) merge low
  LOW_M_RAW=$(
    $TIMECMD $BUILDPATH/motivo-merge \
      --output "${OUTPUT}-Low.${k}" --compress-threshold "$COMP_THR" \
      "${OUTPUT}-Low.${k}.cnt" \
    2>&1 | tee "$OUTPUT.mergeL${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  LOW_M=$(format_time "$LOW_M_RAW")

  # stampa in un’unica riga
  printf "low\t%d\t%s\t%s\n" "$k" "$LOW_B" "$LOW_M"

  # pulizia
  rm -f "${OUTPUT}-Low.${k}.info" "${OUTPUT}-Low.${k}.rts"
done

# -------------------------------------------------------------------------
# 6) workflow ipergrafo su High (k=2..MAXSIZE)
# -------------------------------------------------------------------------
echo -e "step\tk\tbuild\tmerge1\tnws\tmerge2"  # intestazione per la sezione high
for ((k=2; k<=MAXSIZE; k++)); do
  # 6.1) build high
  HIGH_B_RAW=$(
    $TIMECMD $BUILDPATH/motivo-build \
      --hyper --graph "${OUTPUT}-High" --size "$k" \
      --tables-basename "${OUTPUT}-High" \
      --output "${OUTPUT}-High" \
      ${SEED:+--seed "$SEED"} \
    2>&1 | tee "$OUTPUT.buildH${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  HIGH_B=$(format_time "$HIGH_B_RAW")

  # 6.2) merge1 high
  HIGH_M1_RAW=$(
    $TIMECMD $BUILDPATH/motivo-merge \
      --output "${OUTPUT}-High.${k}" --compress-threshold "$COMP_THR" \
      "${OUTPUT}-High.${k}.cnt" \
    2>&1 | tee "$OUTPUT.merge1H${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  HIGH_M1=$(format_time "$HIGH_M1_RAW")

  # 6.3) nws high
  HIGH_NWS_RAW=$(
    $TIMECMD $BUILDPATH/motivo-nws \
      --graph "${OUTPUT}-High" --size "$k" \
      -i "${OUTPUT}-High" --output "${OUTPUT}-High" --threads "$THREADS"\
    2>&1 | tee "$OUTPUT.nwsH${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  HIGH_NWS=$(format_time "$HIGH_NWS_RAW")

  # 6.4) merge-ie high
  HIGH_M2_RAW=$(
    $TIMECMD $BUILDPATH/motivo-merge \
      -e --output "${OUTPUT}-High.${k}.ie" --compress-threshold "$COMP_THR" \
      "${OUTPUT}-High.${k}.ie.cnt" \
    2>&1 | tee "$OUTPUT.merge2H${k}.log" | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
  )
  HIGH_M2=$(format_time "$HIGH_M2_RAW")

  # stampa in un’unica riga
  printf "high\t%d\t%s\t%s\t%s\t%s\n" "$k" "$HIGH_B" "$HIGH_M1" "$HIGH_NWS" "$HIGH_M2"

  # pulizia
  rm -f "${OUTPUT}-High.${k}".{ie.cnt,info,rts}
done

echo "[$(date)] Done." | tee -a "$LOGFILE"