#!/usr/bin/env bash
set -euo pipefail

# --- CONFIGURAZIONE DI BASE ---
BUILDPATH=../build/bin

# Trova un comando 'time' GNU-compatibile
if command -v gtime >/dev/null 2>&1; then
    TIMECMD="gtime --verbose"
elif time --version >/dev/null 2>&1; then
    TIMECMD="time --verbose"
else
    TIMECMD="time"
fi

# --- helper: formatta “h:mm:ss” o “m:ss” in “s” o “.ss” o “m.ss” ---
format_time() {
  local raw=$1
  # isola la parte dopo l'ultimo “:”
  local sec="${raw##*:}"
  # rimuove eventuale zero iniziale
  sec="${sec#0}"
  # se è vuoto, vuol dire “0”
  [[ -z "$sec" ]] && sec="0"
  echo "$sec"
}

# --- FUNZIONE DI USO ---
usage(){
  cat <<EOF
Usage: $0 -g GRAPH -k SIZE -o OUTPUT [-t THREADS] [-c COMPRESS_THRESHOLD]

  -g|--graph                basename del grafo (iper-grafo)
  -k|--size                 taglia massima k (da 1 a k)
  -o|--output               basename per i file di output
  -t|--threads  THREADS     numero di thread (default 1)
  -c|--compress-threshold   soglia di compressione per motivo-merge (default 0)
EOF
  exit 1
}

# --- PARSING ARGOMENTI ---
THREADS=1
COMPRESS_THRESHOLD=0
SEED=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -g|--graph)              GRAPH=$2; shift 2 ;;
    -k|--size)               SIZE=$2; shift 2 ;;
    -o|--output)             OUTPUT=$2; shift 2 ;;
    -t|--threads)            THREADS=$2; shift 2 ;;
    -c|--compress-threshold) COMPRESS_THRESHOLD=$2; shift 2 ;;
    --seed)                  SEED=$2; shift 2;;
    -h|--help)               usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

: "${GRAPH:?Missing -g/--graph}"; : "${SIZE:?Missing -k/--size}"; : "${OUTPUT:?Missing -o/--output}"

LOGFILE="$OUTPUT.log"
echo "[$(date)] Start hypergraph workflow" | tee "$LOGFILE"
printf "k\tbuild\tmerge1\tnws\tmerge2\n"

# --- CICLO PRINCIPALE ---
for ((k=1; k<=SIZE; k++)); do
  printf "%d\t" "$k"

  # 1) BUILD ipergrafo
  echo "[$(date)] motivo-build --hyper size $k" >> "$LOGFILE"
  $TIMECMD $BUILDPATH/motivo-build \
    --hyper \
    --graph "$GRAPH" \
    --size "$k" \
    --colors "$SIZE" \
    --tables-basename "$OUTPUT" \
    --output "$OUTPUT" \
    --threads "$THREADS" \
    --seed "$SEED" \
    > "$OUTPUT.b${k}.log" 2>&1

  # Estrai e formatta wall-time
  BUILD_RAW=$(grep "Elapsed (wall clock) time" "$OUTPUT.b${k}.log" | tail -1 | awk '{print $NF}')
  BUILD_T=$(format_time "$BUILD_RAW")
  printf "%s\t" "$BUILD_T"

  # 2) MERGE normale
  echo "[$(date)] motivo-merge size $k" >> "$LOGFILE"
  $TIMECMD $BUILDPATH/motivo-merge \
    --output "${OUTPUT}.${k}" \
    --compress-threshold "$COMPRESS_THRESHOLD" \
    "${OUTPUT}.${k}.cnt" \
    > "$OUTPUT.m${k}.log" 2>&1

  MERGE1_RAW=$(grep "Elapsed (wall clock) time" "$OUTPUT.m${k}.log" | tail -1 | awk '{print $NF}')
  MERGE1_T=$(format_time "$MERGE1_RAW")
  printf "%s\t" "$MERGE1_T"

  # 3) NWS
  echo "[$(date)] motivo-nws size $k" >> "$LOGFILE"
  $TIMECMD $BUILDPATH/motivo-nws \
    --graph "$GRAPH" \
    --size "${k}" \
    -i "${OUTPUT}" \
    --output "${OUTPUT}" \
    > "$OUTPUT.n${k}.log" 2>&1

  NWS_RAW=$(grep "Elapsed (wall clock) time" "$OUTPUT.n${k}.log" | tail -1 | awk '{print $NF}')
  NWS_T=$(format_time "$NWS_RAW")
  printf "%s\t" "$NWS_T"

  # 4) MERGE delle tabelle NWS
  echo "[$(date)] motivo-merge (NWS) size $k" >> "$LOGFILE"
  $TIMECMD $BUILDPATH/motivo-merge \
    -e \
    --output "${OUTPUT}.${k}.ie" \
    --compress-threshold "$COMPRESS_THRESHOLD" \
    "${OUTPUT}.${k}.ie.cnt" \
    > "$OUTPUT.i${k}.log" 2>&1

  MERGE2_RAW=$(grep "Elapsed (wall clock) time" "$OUTPUT.i${k}.log" | tail -1 | awk '{print $NF}')
  MERGE2_T=$(format_time "$MERGE2_RAW")
  printf "%s\n" "$MERGE2_T"

  # Pulizia intermedi
  # rm -f "${OUTPUT}.${k}.cnt" "${OUTPUT}.${k}ie..cnt"
done

echo "[$(date)] Done." | tee -a "$LOGFILE"