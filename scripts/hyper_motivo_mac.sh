#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# motivo-hyper pipeline (motivo-hyper-build per HIGH; motivo-build per Gaifman)
# ------------------------------------------------------------

BUILDPATH=${BUILDPATH:-../build/bin}

# pick a GNU-like time
if command -v gtime &>/dev/null; then
  TIMECMD="gtime --verbose"
elif time --version &>/dev/null; then
  TIMECMD="time --verbose"
else
  TIMECMD="time"
fi

format_time(){ # get seconds from h:mm:ss or m:ss (prints seconds' field)
  local raw=$1
  local s="${raw##*:}"
  s="${s#0}"
  [[ -z "$s" ]] && s="0"
  echo "$s"
}

run_timed(){ # logs + returns the "Elapsed (wall clock)" field
  local log="$1"; shift
  $TIMECMD "$@" 2>&1 | tee "$log" \
    | grep "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}'
}

usage(){
  cat <<EOF
Usage: $0 -g GRAPH -k MAXSIZE -o OUTPUT [-t THREADS] [-c COMP_THR] [-T THRESHOLD] [--seed SEED] [--colors C] [-S SAMPLES]

  -g|--graph            input hypergraph basename (without extension)
  -k|--maxsize          max k (>=2)
  -o|--output           GLOBAL tables basename (also used as -i for k>=2)
  -t|--threads THREADS  threads for hyper-build/NWS (default 1)
  -c|--comp-thr COMP    merge compression threshold (default 0)
  -T|--threshold THR    hgsplit threshold (default 0; if omitted, auto-α)
  --seed SEED           RNG seed for k=1 (optional)
  --colors C            number of colors for k=1 (default: MAXSIZE)
  -S|--samples N        if >0, run final sampling at k=MAXSIZE
EOF
  exit 1
}

# ------------------------- args ------------------------------
THREADS=1
COMP_THR=0
THRESHOLD=0
SEED=""
SAMPLES=""
COLORS=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -g|--graph)     GRAPH=$2;       shift 2 ;;
    -k|--maxsize)   MAXSIZE=$2;     shift 2 ;;
    -o|--output)    OUTPUT=$2;      shift 2 ;;
    -t|--threads)   THREADS=$2;     shift 2 ;;
    -c|--comp-thr)  COMP_THR=$2;    shift 2 ;;
    -T|--threshold) THRESHOLD=$2;   shift 2 ;;
    --seed)         SEED=$2;        shift 2 ;;
    --colors)       COLORS=$2;      shift 2 ;;
    -S|--samples)   SAMPLES=$2;     shift 2 ;;
    -h|--help)      usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

: "${GRAPH:?Missing -g/--graph}"
: "${MAXSIZE:?Missing -k/--maxsize}"
: "${OUTPUT:?Missing -o/--output}"
[[ -n "${COLORS}" ]] || COLORS="${MAXSIZE}"

LOGDIR="$(dirname -- "$OUTPUT")"
mkdir -p "$LOGDIR"
LOGFILE="$OUTPUT.log"

echo "[$(date)] motivo-hyper start" | tee "$LOGFILE"

# --------------------- basenames -----------------------------
HIGH_GRAPH="${OUTPUT}-High"     # ipergrafo HIGH (basename)
LOW_GRAPH="${OUTPUT}-Low"       # ipergrafo LOW  (basename)
LOW_G="${LOW_GRAPH}"            # gaifman(LOW): input=output

HIGH_TTC="${OUTPUT}-HighTTC"    # TTC per HIGH
LOW_TTC="${OUTPUT}-LowTTC"      # TTC per LOW
GLOBAL_TTC="${OUTPUT}"          # TTC GLOBAL (merge e input per k>=2)

# ----------------- 1) split + gaifman(LOW) -------------------
printf "hgsplit\t\t"
SPLIT_RAW=$(run_timed "$OUTPUT.split.log" \
  "$BUILDPATH/motivo-hgsplit" \
    -i "$GRAPH" -s "$LOW_GRAPH" -l "$HIGH_GRAPH" )
printf "%s\n" "$(format_time "$SPLIT_RAW")"

printf "gaifman(low)\t"
GAIF_RAW=$(run_timed "$OUTPUT.gaifman.log" \
  "$BUILDPATH/motivo-gaifman" \
    --input "$LOW_G" --output "$LOW_G" -j "$THREADS")
printf "%s\n" "$(format_time "$GAIF_RAW")"

# ----------------- 2) k=1 (build su Gaifman LOW) -------------
echo -e "step\tk\tbuild\tmerge\tnws\tmerge-ie"

# 2.1 k=1 build (grafi: motivo-build sul Gaifman LOW)
printf "high(k=1)\t1\t"
H1_B_RAW=$(run_timed "$OUTPUT.buildH1.log" \
  "$BUILDPATH/motivo-build" \
    --graph "$LOW_G" --size 1 \
    --colors "$COLORS" \
    --output "$HIGH_TTC" \
    ${SEED:+--seed "$SEED"} --threads "$THREADS") 
printf "%s\t" "$(format_time "$H1_B_RAW")"

# 2.2 merge HIGH_TTC.1
H1_M_RAW=$(run_timed "$OUTPUT.mergeH1.log" \
  "$BUILDPATH/motivo-merge" \
    --output "$HIGH_TTC.1" --compress-threshold "$COMP_THR" \
    "$HIGH_TTC.1.cnt")
printf "%s\t" "$(format_time "$H1_M_RAW")"

# 2.3 NWS su HIGH (ipergrafo)
H1_NWS_RAW=$(run_timed "$OUTPUT.nwsH1.log" \
  "$BUILDPATH/motivo-nws" \
    --graph "$HIGH_GRAPH" --size 1 \
    -i "$HIGH_TTC" --output "$HIGH_TTC" --threads "$THREADS")
printf "%s\t" "$(format_time "$H1_NWS_RAW")"

# 2.4 IE merge -> GLOBAL_TTC.1.ie
H1_IE_RAW=$(run_timed "$OUTPUT.nwsMerge1.log" \
  "$BUILDPATH/motivo-merge" \
    -e --output "$GLOBAL_TTC.1.ie" --compress-threshold "$COMP_THR" \
    "$HIGH_TTC.1.ie.cnt")
printf "%s\n" "$(format_time "$H1_IE_RAW")"

# 2.5 GLOBAL k=1 = HIGH k=1 (evita doppia colorazione)
printf "merge-global-1\t"
MERGE1_RAW=$(run_timed "$OUTPUT.mergeGlobal1.log" \
  "$BUILDPATH/motivo-merge" \
    --output "$GLOBAL_TTC.1" --compress-threshold "$COMP_THR" \
    "$HIGH_TTC.1.cnt")
printf "%s\n" "$(format_time "$MERGE1_RAW")"

# ----------------- 3) k=2..MAXSIZE ---------------------------
echo -e "k\tH.build\tH.merge\tL.build\tL.merge\tLH.fuse\tLH.merge\tNWS\tIE.merge"

for ((k=2; k<=MAXSIZE; k++)); do
  # 3.1 HIGH (ipergrafo): motivo-hyper-build con TTC low&ie dalla GLOBAL
  H_B_RAW=$(run_timed "$OUTPUT.buildH${k}.log" \
    "$BUILDPATH/motivo-hyper-build" \
      --graph "$HIGH_GRAPH" \
      --size "$k" \
      --output "$HIGH_TTC" \
      --lower "$GLOBAL_TTC" \
      --ie    "$GLOBAL_TTC" \
      --normalize false \
      --threads "$THREADS")
  H_B=$(format_time "$H_B_RAW")

  H_M_RAW=$(run_timed "$OUTPUT.merge1H${k}.log" \
    "$BUILDPATH/motivo-merge" \
      --output "$HIGH_TTC.${k}" --compress-threshold "$COMP_THR" \
      "$HIGH_TTC.${k}.cnt")
  H_M=$(format_time "$H_M_RAW")

  # 3.2 LOW (Gaifman): motivo-build (grafi), stessa colorazione globale
  L_B_RAW=$(run_timed "$OUTPUT.buildL${k}.log" \
    "$BUILDPATH/motivo-build" \
      --graph "$LOW_G" --size "$k" \
      --output "$LOW_TTC" \
      -i "$GLOBAL_TTC" --normalize false --threads "$THREADS")
  L_B=$(format_time "$L_B_RAW")

  L_M_RAW=$(run_timed "$OUTPUT.mergeL${k}.log" \
    "$BUILDPATH/motivo-merge" \
      --output "$LOW_TTC.${k}" --compress-threshold "$COMP_THR" \
      "$LOW_TTC.${k}.cnt")
  L_M=$(format_time "$L_M_RAW")

  # 3.3 fuse LOW+HIGH -> GLOBAL.${k}
  LH_FUSE_RAW=$(run_timed "$OUTPUT.lowHighFuse${k}.log" \
    "$BUILDPATH/motivo-low-high-merge" \
      --low "$LOW_TTC.${k}.cnt" --high "$HIGH_TTC.${k}.cnt" \
      -o "$GLOBAL_TTC.${k}" -c "$LOW_G")
  LH_FUSE=$(format_time "$LH_FUSE_RAW")

  LH_M_RAW=$(run_timed "$OUTPUT.lowHighMerge${k}.log" \
    "$BUILDPATH/motivo-merge" \
      --output "$GLOBAL_TTC.${k}" --compress-threshold "$COMP_THR" \
      "$GLOBAL_TTC.${k}.cnt")
  LH_M=$(format_time "$LH_M_RAW")

  # 3.4 NWS su HIGH (ipergrafo) e IE-merge in GLOBAL
  NWS_RAW=$(run_timed "$OUTPUT.nwsH${k}.log" \
    "$BUILDPATH/motivo-nws" \
      --graph "$HIGH_GRAPH" --size "$k" \
      -i "$GLOBAL_TTC" --output "$HIGH_TTC" --threads "$THREADS")
  NWS=$(format_time "$NWS_RAW")

  IE_RAW=$(run_timed "$OUTPUT.merge2H${k}.log" \
    "$BUILDPATH/motivo-merge" \
      -e --output "$GLOBAL_TTC.${k}.ie" --compress-threshold "$COMP_THR" \
      "$HIGH_TTC.${k}.ie.cnt")
  IE=$(format_time "$IE_RAW")

  printf "%d\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "$k" "$H_B" "$H_M" "$L_B" "$L_M" "$LH_FUSE" "$LH_M" "$NWS" "$IE"

  rm -f "$LOW_TTC.${k}.info"  "$LOW_TTC.${k}.rts" \
        "$HIGH_TTC.${k}.info" "$HIGH_TTC.${k}.rts" || true
done

# ----------------- 4) optional sampling ----------------------
if [[ -n "${SAMPLES}" && "${SAMPLES}" -gt 0 ]]; then
  echo -e "step\tk\ttime"
  printf "sample\t%d\t" "$MAXSIZE"
  SMP_RAW=$(run_timed "$OUTPUT.sample${MAXSIZE}.log" \
    "$BUILDPATH/motivo-hyper-sample" \
      --gaifman "$LOW_G" \
      --hypergraph "$HIGH_GRAPH" \
      --hypergraph-full "$GRAPH" \
      --tables "$GLOBAL_TTC" \
      --ttc-low "$LOW_TTC" \
      --ttc-high "$HIGH_TTC" \
      --nws-high "$GLOBAL_TTC" \
      --size "$MAXSIZE" \
      --num-samples "$SAMPLES" \
      --threads "$THREADS" \
      --time-budget 1000000 \
      --group --graphlets --estimate-occurrences \
      ${SEED:+--seed "$SEED"} )
  printf "%s\n" "$(format_time "$SMP_RAW")"
fi

echo "[$(date)] Done." | tee -a "$LOGFILE"