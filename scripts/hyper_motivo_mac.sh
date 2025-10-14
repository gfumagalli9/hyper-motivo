#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# motivo-hyper modular pipeline (LOW/HIGH) + CSV timings
# -g punta a un .txt ipergrafo; si usa motivo-hypergraph per generare .hmeta/.hbin/.hidx
# ------------------------------------------------------------

BUILDPATH=${BUILDPATH:-../build/bin}

# ---------- time(1) detection (GNU) with fallback ----------
if command -v gtime >/dev/null 2>&1; then
  TIMECMD=(gtime --verbose)
elif command -v /usr/bin/time >/dev/null 2>&1 && /usr/bin/time --verbose true >/dev/null 2>&1; then
  TIMECMD=(/usr/bin/time --verbose)
elif command -v time >/dev/null 2>&1 && time --version >/dev/null 2>&1; then
  TIMECMD=(time --verbose)
else
  TIMECMD=()  # fallback: manual timing
fi

# parse "h:mm:ss" or "m:ss" or "s.ss" -> seconds with decimals
to_seconds() {
  awk -F: '
    NF==3 { printf("%.3f", ($1*3600)+($2*60)+$3); next }
    NF==2 { printf("%.3f", ($1*60)+$2); next }
    NF==1 { printf("%.3f", $1); next }
  '
}

# ---------------- CSV helpers ----------------
TIMINGS_CSV=""  # inizializzato dopo il parse
csv_header() {
  echo "date,graph,output,threads,max_k,comp_thr,threshold,stage,k,label,seconds,log" >"$TIMINGS_CSV"
}
csv_row() { # stage, k, label, seconds, log
  local stage="$1" k="$2" label="$3" secs="$4" log="$5"
  local ts; ts="$(date -Iseconds)"
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$ts" "$GRAPH" "$OUTPUT" "$THREADS" "$MAXSIZE" "$COMP_THR" "${THRESHOLD:-}" \
    "$stage" "$k" "$label" "$secs" "$log" >>"$TIMINGS_CSV"
}

# run command; if GNU time -v available parse wall time; else manual timing
run_timed() {
  local log="$1"; shift
  if ((${#TIMECMD[@]})); then
    "${TIMECMD[@]}" "$@" 2>&1 | tee "$log" \
      | grep -F "Elapsed (wall clock) time" | tail -1 | awk '{print $NF}' | to_seconds
  else
    local t0 t1
    t0=$(date +%s.%N)
    "$@" >"$log" 2>&1
    t1=$(date +%s.%N)
    awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.3f\n", (b-a)}'
  fi
}

usage() {
  cat <<EOF
Usage: $0 [--build] [--sample]
          -g|--graph PATH.txt      (file testo ipergrafo)
          -k|--maxsize K           (>=2)
          -o|--output BASENAME     (basename output per tutti gli artifact)
          [-t|--threads N]         (default 1)
          [-c|--comp-thr X]        (threshold compressione merge, default 0)
          [-T|--threshold THR]     (hgsplit threshold; vuoto=auto)
          [--seed S] [--colors C]  (k=1 build, default colors=K)
          [-S|--samples N]         (se >0, esegue hyper-sample a k=K)

Environment:
  BUILDPATH   path ai binari motivo (default ../build/bin)
Outputs:
  - Logs:          \$OUTPUT.*.log
  - Timings (CSV): \$OUTPUT.timings.csv
EOF
  exit 1
}

# ------------------------- args ------------------------------
BUILD=NO
SAMPLE=NO
THREADS=1
COMP_THR=0
THRESHOLD=""
SEED=""
SAMPLES=""
COLORS=""

while [[ $# -gt 0 ]]; do
  case $1 in
    --build)   BUILD=YES; shift ;;
    --sample)  SAMPLE=YES; shift ;;
    -g|--graph)     GRAPH=$2; shift 2 ;;
    -k|--maxsize)   MAXSIZE=$2; shift 2 ;;
    -o|--output)    OUTPUT=$2; shift 2 ;;
    -t|--threads)   THREADS=$2; shift 2 ;;
    -c|--comp-thr)  COMP_THR=$2; shift 2 ;;
    -T|--threshold) THRESHOLD=$2; shift 2 ;;
    --seed)         SEED=$2; shift 2 ;;
    --colors)       COLORS=$2; shift 2 ;;
    -S|--samples)   SAMPLES=$2; shift 2 ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

: "${GRAPH:?Missing -g/--graph (path al .txt ipergrafo)}"
: "${MAXSIZE:?Missing -k/--maxsize}"
: "${OUTPUT:?Missing -o/--output}"
[[ -n "${COLORS}" ]] || COLORS="${MAXSIZE}"
if [[ "$BUILD" == "NO" && "$SAMPLE" == "NO" ]]; then BUILD=YES; SAMPLE=YES; fi
[[ -f "$GRAPH" ]] || { echo "[fatal] Input .txt non trovato: $GRAPH" >&2; exit 2; }

LOGDIR="$(dirname -- "$OUTPUT")"
mkdir -p "$LOGDIR"
LOGFILE="$OUTPUT.log"
TIMINGS_CSV="${OUTPUT}.timings.csv"
csv_header

echo "[$(date)] motivo-hyper start" | tee "$LOGFILE"

# --------------------- basenames -----------------------------
INPUT_GRAPH="${OUTPUT}-Input"   # basename binario creato da motivo-hypergraph
HIGH_GRAPH="${OUTPUT}-High"     # ipergrafo HIGH (basename da hgsplit)
LOW_GRAPH="${OUTPUT}-Low"       # ipergrafo LOW  (basename da hgsplit)
LOW_G="${LOW_GRAPH}"            # gaifman(LOW): input=output

HIGH_TTC="${OUTPUT}-HighTTC"    # TTC per HIGH
LOW_TTC="${OUTPUT}-LowTTC"      # TTC per LOW
GLOBAL_TTC="${OUTPUT}"          # TTC GLOBAL (merge e input per k>=2)

# Helper: verifica se un binario supporta --store-on-0-colored-vertices-only
supports_store_on_0() {
  local bin="$1"
  "$bin" --help 2>&1 | grep -q -- '--store-on-0-colored-vertices-only'
}

HAS_STORE_LOW="no"
HAS_STORE_HIGH="no"
if supports_store_on_0 "$BUILDPATH/motivo-build"; then HAS_STORE_LOW="yes"; fi
if supports_store_on_0 "$BUILDPATH/motivo-hyper-build"; then HAS_STORE_HIGH="yes"; fi

# --------- rilevo i flag supportati da motivo-hypergraph ----------
detect_hypergraph_flags() {
  local help
  help="$("$BUILDPATH/motivo-hypergraph" --help 2>&1 || true)"
  if grep -q -- "--input" <<<"$help"; then
    HG_IN="--input"
  elif grep -q -E '\s-i[ ,]' <<<"$help"; then
    HG_IN="-i"
  else
    echo "[fatal] Non riesco a trovare il flag input per motivo-hypergraph (né --input né -i)" >&2
    exit 3
  fi
  if grep -q -- "--output" <<<"$help"; then
    HG_OUT="--output"
  elif grep -q -E '\s-o[ ,]' <<<"$help"; then
    HG_OUT="-o"
  else
    echo "[fatal] Non riesco a trovare il flag output per motivo-hypergraph (né --output né -o)" >&2
    exit 3
  fi
}
detect_hypergraph_flags

# ------------------------- BUILD -----------------------------
build() {
  echo -e "step\t\tsecs"

  # 0) txt -> bin (.hmeta/.hbin/.hidx)
  local LOG HGSEC
  LOG="$OUTPUT.hypergraph.log"
  printf "hypergraph\t"
  HGSEC=$(run_timed "$LOG" "$BUILDPATH/motivo-hypergraph" "$HG_IN" "$GRAPH" "$HG_OUT" "$INPUT_GRAPH")
  csv_row "prep" "" "hypergraph_build" "$HGSEC" "$LOG"
  printf "%s\n" "$HGSEC"

  # 1) split HIGH/LOW
  local SPLIT
  LOG="$OUTPUT.split.log"
  printf "hgsplit\t\t"
  SPLIT=$(run_timed "$LOG" \
    "$BUILDPATH/motivo-hgsplit" -i "$INPUT_GRAPH" -s "$LOW_GRAPH" -l "$HIGH_GRAPH" ${THRESHOLD:+-t "$THRESHOLD"})
  csv_row "split" "" "hgsplit" "$SPLIT" "$LOG"
  printf "%s\n" "$SPLIT"

  # 2) gaifman(LOW)
  LOG="$OUTPUT.gaifman.log"
  printf "gaifman(low)\t"
  GF=$(run_timed "$LOG" "$BUILDPATH/motivo-gaifman" --input "$LOW_G" --output "$LOW_G" -j "$THREADS")
  csv_row "split" "" "gaifman_low" "$GF" "$LOG"
  printf "%s\n" "$GF"

  # -------- k=1 (tabella unificata) --------
  LOG="$OUTPUT.buildH1.log"
  B1=$(run_timed "$LOG" "$BUILDPATH/motivo-build" \
        --graph "$LOW_G" --size 1 \
        --colors "$COLORS" \
        --output "$HIGH_TTC" \
        ${SEED:+--seed "$SEED"} --threads "$THREADS")
  csv_row "k1" "1" "build_high_k1" "$B1" "$LOG"

  LOG="$OUTPUT.mergeH1.log"
  M1=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
        --output "$HIGH_TTC.1" --compress-threshold "$COMP_THR" \
        "$HIGH_TTC.1.cnt")
  csv_row "k1" "1" "merge_high_k1" "$M1" "$LOG"

  LOG="$OUTPUT.nwsH1.log"
  N1=$(run_timed "$LOG" "$BUILDPATH/motivo-nws" \
        --graph "$HIGH_GRAPH" --size 1 \
        -i "$HIGH_TTC" --output "$HIGH_TTC" --threads "$THREADS")
  csv_row "k1" "1" "nws_high_k1" "$N1" "$LOG"

  LOG="$OUTPUT.nwsMerge1.log"
  I1=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
        -e --output "$GLOBAL_TTC.1.ie" --compress-threshold "$COMP_THR" \
        "$HIGH_TTC.1.ie.cnt")
  csv_row "k1" "1" "merge_ie_global_k1" "$I1" "$LOG"

  # Per semplicità di naming per k>=2 (LOWER/IE): copia HIGH_TTC.1.* in GLOBAL_TTC.1.*
  cp -f "$HIGH_TTC.1.dtz"  "$GLOBAL_TTC.1.dtz"
  cp -f "$HIGH_TTC.1.info" "$GLOBAL_TTC.1.info"
  cp -f "$HIGH_TTC.1.rts"  "$GLOBAL_TTC.1.rts"

  # -------- tabella unificata: k=1 + loop --------
  echo -e "k\tH.build\tL.build\tLH.fuse\tLH.merge\tNWS\tIE.merge"
  # k=1: L.build/LH.fuse non applicano ('/'); LH.merge = M1
  printf "1\t%s\t%s\t%s\t%s\t\t%s\t%s\n" \
    "$(printf '%.3f' "$B1")" "/" "/" \
    "$(printf '%.3f' "$M1")" "$(printf '%.3f' "$N1")" "$(printf '%.3f' "$I1")"

  # -------- k = 2..MAXSIZE --------
  for ((k=2; k<=MAXSIZE; k++)); do
    local H_STORE_FLAG="" L_STORE_FLAG=""
    if (( k == MAXSIZE )); then
      [[ "$HAS_STORE_HIGH" == "yes" ]] && H_STORE_FLAG="--store-on-0-colored-vertices-only" \
        && echo "[info] Enabling store-on-0 for HIGH at k=$k" | tee -a "$LOGFILE" \
        || echo "[warn] motivo-hyper-build lacks --store-on-0-colored-vertices-only" | tee -a "$LOGFILE"
      [[ "$HAS_STORE_LOW"  == "yes" ]] && L_STORE_FLAG="--store-on-0-colored-vertices-only" \
        && echo "[info] Enabling store-on-0 for LOW  at k=$k" | tee -a "$LOGFILE" \
        || echo "[warn] motivo-build lacks --store-on-0-colored-vertices-only" | tee -a "$LOGFILE"
    fi

    LOG="$OUTPUT.buildH${k}.log"
    HB=$(run_timed "$LOG" "$BUILDPATH/motivo-hyper-build" \
          --graph "$HIGH_GRAPH" \
          --size "$k" \
          --output "$HIGH_TTC" \
          --lower "$GLOBAL_TTC" \
          --ie    "$GLOBAL_TTC" \
          --normalize false \
          --threads "$THREADS" \
          ${H_STORE_FLAG:+$H_STORE_FLAG})
    csv_row "kloop" "$k" "build_high" "$HB" "$LOG"

    LOG="$OUTPUT.buildL${k}.log"
    LB=$(run_timed "$LOG" "$BUILDPATH/motivo-build" \
          --graph "$LOW_G" --size "$k" \
          --output "$LOW_TTC" \
          -i "$GLOBAL_TTC" --normalize false --threads "$THREADS" \
          ${L_STORE_FLAG:+$L_STORE_FLAG})
    csv_row "kloop" "$k" "build_low" "$LB" "$LOG"

    LOG="$OUTPUT.lowHighFuse${k}.log"
    FH=$(run_timed "$LOG" "$BUILDPATH/motivo-low-high-merge" \
          --low "$LOW_TTC.${k}.cnt" --high "$HIGH_TTC.${k}.cnt" \
          -o "$GLOBAL_TTC.${k}" -c "$LOW_G")
    csv_row "kloop" "$k" "fuse_LH" "$FH" "$LOG"

    LOG="$OUTPUT.lowHighMerge${k}.log"
    MH=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
          --output "$GLOBAL_TTC.${k}" --compress-threshold "$COMP_THR" \
          "$GLOBAL_TTC.${k}.cnt")
    csv_row "kloop" "$k" "merge_LH" "$MH" "$LOG"

    LOG="$OUTPUT.nwsH${k}.log"
    NW=$(run_timed "$LOG" "$BUILDPATH/motivo-nws" \
          --graph "$HIGH_GRAPH" --size "$k" \
          -i "$GLOBAL_TTC" --output "$HIGH_TTC" --threads "$THREADS")
    csv_row "kloop" "$k" "nws_high" "$NW" "$LOG"

    LOG="$OUTPUT.merge2H${k}.log"
    IE=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
          -e --output "$GLOBAL_TTC.${k}.ie" --compress-threshold "$COMP_THR" \
          "$HIGH_TTC.${k}.ie.cnt")
    csv_row "kloop" "$k" "merge_ie" "$IE" "$LOG"

    printf "%d\t%s\t%s\t%s\t%s\t\t%s\t%s\n" \
      "$k" "$(printf '%.3f' "$HB")" "$(printf '%.3f' "$LB")" \
      "$(printf '%.3f' "$FH")" "$(printf '%.3f' "$MH")" \
      "$(printf '%.3f' "$NW")" "$(printf '%.3f' "$IE")"

    rm -f "$LOW_TTC.${k}.info"  "$LOW_TTC.${k}.rts" \
          "$HIGH_TTC.${k}.info" "$HIGH_TTC.${k}.rts" || true
  done
}

# ------------------------- SAMPLE ----------------------------
sample() {
  [[ -n "${SAMPLES}" && "${SAMPLES}" -gt 0 ]] || { echo "[info] no sampling requested"; return; }
  echo -e "step\tk\ttime"
  printf "sample\t%d\t" "$MAXSIZE"

  local SAMPLE_BASE="${OUTPUT}.sample${MAXSIZE}"
  local LOG="$OUTPUT.sample${MAXSIZE}.log"
  SMP=$(run_timed "$LOG" "$BUILDPATH/motivo-hyper-sample" \
      --gaifman "$LOW_G" \
      --hypergraph "$HIGH_GRAPH" \
      --hypergraph-full "$INPUT_GRAPH" \
      --tables "$GLOBAL_TTC" \
      --nws-high "$GLOBAL_TTC" \
      --size "$MAXSIZE" \
      --num-samples "$SAMPLES" \
      --threads "$THREADS" \
      --group --graphlets --estimate-occurrences --canonicize --spanning-trees --vertices \
      ${SEED:+--seed "$SEED"} \
      --output "$SAMPLE_BASE")
  csv_row "sample" "$MAXSIZE" "hyper_sample" "$SMP" "$LOG"
  printf "%s\n" "$SMP"

  echo "Samples are in ${SAMPLE_BASE}.csv:"
  head -6 "${SAMPLE_BASE}.csv" || true
}

# ------------------------- run -------------------------------
if [[ "$BUILD" == "YES" ]]; then build; fi
if [[ "$SAMPLE" == "YES" ]]; then sample; fi

echo "[$(date)] Done." | tee -a "$LOGFILE"