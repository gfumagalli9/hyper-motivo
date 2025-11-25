#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# motivo-hyper modular pipeline (LOW/HIGH) + CSV timings
# -g punta al basename di un ipergrafo già costruito (.hmeta/.hbin/.hidx)
# In aggiunta a $OUTPUT.timings.csv, produce $OUTPUT.perf con:
# output,graph,size,colors,compress_threshold,step,ntreelets,nsamples,nthreads,walltime,usertime,systemtime,actualtime
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

# parse "h:mm:ss" or "m:ss" or "s.ss" -> seconds with 2 decimals
to_seconds() {
  awk -F: '
    NF==3 { printf("%.2f", ($1*3600)+($2*60)+$3); next }
    NF==2 { printf("%.2f", ($1*60)+$2); next }
    NF==1 { printf("%.2f", $1); next }
  '
}

# ---------------- CSV helpers (timings modulare) ----------------
TIMINGS_CSV=""
csv_header() {
  echo "date,graph,output,threads,max_k,comp_thr,threshold,stage,k,label,seconds,log" >"$TIMINGS_CSV"
}
csv_row() { # stage, k, label, seconds, log
  local stage="$1" k="$2" label="$3" secs="$4" log="$5"
  local ts; ts="$(date -Iseconds)"
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$ts" "${GRAPH:-}" "$OUTPUT" "$THREADS" "$MAXSIZE" "$COMP_THR" "${THRESHOLD:-}" \
    "$stage" "$k" "$label" "$secs" "$log" >>"$TIMINGS_CSV"
}

# ---------------- CSV stile "grafi" per ogni step ----------------
PERF_CSV=""
perf_header(){
  echo "output,graph,size,colors,compress_threshold,step,ntreelets,nsamples,nthreads,walltime,usertime,systemtime,actualtime" > "$PERF_CSV"
}
perf_row(){ # args: out graph size colors comp_thr step ntree nsamples nth wall u s act
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$1" "$2" "$3" "$4" "$5" "$6" "${7:-0}" "${8:-0}" "${9:-0}" "${10:-0.00}" "${11:-0.00}" "${12:-0.00}" "${13:-0.00}" \
    >> "$PERF_CSV"
}

# Parsers da log di GNU time -v / tool output
grep_user_time(){ grep -Eo "User time \(seconds\):[[:space:]]*[0-9.]+$" "$1" | awk '{print $4}' | head -1 | awk '{printf "%.2f\n",$1+0}' || echo "0.00"; }
grep_sys_time(){  grep -Eo "System time \(seconds\):[[:space:]]*[0-9.]+$" "$1" | awk '{print $4}' | head -1 | awk '{printf "%.2f\n",$1+0}' || echo "0.00"; }
grep_elapsed(){   grep -F "Elapsed (wall clock) time" "$1" | tail -1 | awk '{print $NF}' | to_seconds || echo "0.00"; }
grep_actual(){    grep -Eo "^(Building|Merge|Sampling) time: [0-9.]+ s$" "$1" | tail -1 | awk '{print $3}' | awk '{printf "%.2f\n",$1+0}' || true; }
grep_ntree(){     grep -Eo "^Total number of treelet occurrences: [0-9]+" "$1" | awk '{print $NF}' | head -1 || echo "0"; }
grep_threads(){   grep -Eo "using [0-9]+ thread\(s\)$" "$1" | awk '{print $2}' | head -1 || echo ""; }
grep_nsamples(){  grep -Eo 'took[[:space:]]+[0-9]+' "$1" | awk '{print $2}' | tail -1 || true; }

emit_step_perf(){ # label k log wall default_threads is_merge is_sample
  local label="$1" k="$2" log="$3" wall="$4" dflt_th="$5" is_merge="$6" is_sample="$7"
  local u s a nth nt ns
  u="$(grep_user_time "$log")"; s="$(grep_sys_time "$log")"
  a="$(grep_actual "$log")"; a="${a:-$wall}"
  nth="$(grep_threads "$log")"; nth="${nth:-$dflt_th}"
  if [[ "$is_merge" == "1" ]]; then nt="$(grep_ntree "$log")"; else nt="0"; fi
  if [[ "$is_sample" == "1" ]]; then ns="$(grep_nsamples "$log")"; ns="${ns:-$SAMPLES}"; else ns="0"; fi
  perf_row "$OUTPUT" "$LOW_G" "$k" "$COLORS" "$COMP_THR" "$label" "$nt" "$ns" "$nth" "$wall" "$u" "$s" "$a"
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
    awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f\n", (b-a)}'
  fi
}

usage() {
  cat <<EOF
Usage: $0 [--build] [--sample]
          -g|--graph BASENAME      (basename ipergrafo già costruito: BASENAME.hmeta/.hbin/.hidx)
          -k|--maxsize K           (>=2)
          -o|--output BASENAME     (basename output per tutti gli artifact)
          [-t|--threads N]         (default 1)
          [-c|--comp-thr X]        (threshold compressione merge, default 0)
          [-T|--threshold THR]     (hgsplit threshold; vuoto=auto)
          [--seed S] [--colors C]  (k=1 build, default colors=K)
          [-S|--samples N]         (se >0, esegue hyper-sample a k=K)
          [-L|--low-base BASENAME] (optional: precomputed LOW Gaifman basename)
          [-H|--high-base BASENAME](optional: pre-split HIGH hypergraph basename)
          [--no-subtype-pruning]   (propaga a motivo-nws per disattivare early-stop sui sottotipi)

Notes:
  - If both --low-base (Gaifman) and --high-base are provided, the script skips
    hgsplit and gaifman(low) and uses those basenames directly.

Outputs:
  - Logs:          \$OUTPUT.*.log
  - Timings (CSV): \$OUTPUT.timings.csv
  - Steps CSV:     \$OUTPUT.perf
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
GRAPH=""        # optional if pre-split is used
PRE_LOW_GAIF="" # NEW: Gaifman(LOW) basename (precomputed)
PRE_HIGH=""     # NEW: HIGH hypergraph basename (pre-split)
NO_SUBTYPE_PRUNING=""  # if set to "--no-subtype-pruning", viene passato a motivo-nws

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
    -L|--low-base)  PRE_LOW_GAIF=$2; shift 2 ;;  # expects Gaifman(LOW) basename
    -H|--high-base) PRE_HIGH=$2; shift 2 ;;      # expects HIGH hypergraph basename
    --no-subtype-pruning) NO_SUBTYPE_PRUNING="--no-subtype-pruning"; shift ;;
    -h|--help) usage ;;
    *) echo "Unknown option: $1"; usage ;;
  esac
done

: "${MAXSIZE:?Missing -k/--maxsize}"
: "${OUTPUT:?Missing -o/--output}"
[[ -n "${COLORS}" ]] || COLORS="${MAXSIZE}"
if [[ "$BUILD" == "NO" && "$SAMPLE" == "NO" ]]; then BUILD=YES; SAMPLE=YES; fi

# Validate inputs: either (both pre-split) or (-g present)
USE_PRESPLIT="no"
if [[ -n "$PRE_LOW_GAIF" || -n "$PRE_HIGH" ]]; then
  [[ -n "$PRE_LOW_GAIF" && -n "$PRE_HIGH" ]] || { echo "[fatal] Provide BOTH --low-base (Gaifman) and --high-base (hypergraph) or none"; exit 2; }
  USE_PRESPLIT="yes"
else
  [[ -n "$GRAPH" ]] || { echo "[fatal] Missing -g/--graph (or provide --low-base AND --high-base)"; exit 2; }
  [[ -f "${GRAPH}.hmeta" ]] || { echo "[fatal] Non trovo ${GRAPH}.hmeta (ipergrafo non presente)" >&2; exit 2; }
fi

LOGDIR="$(dirname -- "$OUTPUT")"
mkdir -p "$LOGDIR"
LOGFILE="$OUTPUT.log"
TIMINGS_CSV="${OUTPUT}.timings.csv"
PERF_CSV="${OUTPUT}.perf"
csv_header
perf_header

echo "[$(date)] motivo-hyper start" | tee "$LOGFILE"

# --------------------- basenames -----------------------------
HIGH_GRAPH="${OUTPUT}-High"     # ipergrafo HIGH (by default from hgsplit)
LOW_GRAPH="${OUTPUT}-Low"       # ipergrafo LOW  (by default from hgsplit)
LOW_G="${LOW_GRAPH}"            # this will be the Gaifman of LOW after gaifman step

HIGH_TTC="${OUTPUT}-HighTTC"    # TTC per HIGH
LOW_TTC="${OUTPUT}-LowTTC"      # TTC per LOW
GLOBAL_TTC="${OUTPUT}"          # TTC GLOBAL (merge e input per k>=2)

# If pre-split was supplied, override basenames:
# - HIGH_GRAPH = provided HIGH hypergraph
# - LOW_G     = provided LOW Gaifman (already a graph)
if [[ "$USE_PRESPLIT" == "yes" ]]; then
  HIGH_GRAPH="$PRE_HIGH"
  LOW_G="$PRE_LOW_GAIF"
fi

# Helper: verifica se un binario supporta --store-on-0-colored-vertices-only
supports_store_on_0() {
  local bin="$1"
  "$bin" --help 2>&1 | grep -q -- '--store-on-0-colored-vertices-only'
}

HAS_STORE_LOW="no"
HAS_STORE_HIGH="no"
if supports_store_on_0 "$BUILDPATH/motivo-build"; then HAS_STORE_LOW="yes"; fi
if supports_store_on_0 "$BUILDPATH/motivo-hyper-build"; then HAS_STORE_HIGH="yes"; fi

# ------------------------- BUILD -----------------------------
build() {
  echo -e "step\t\tsecs"

  # 1) split HIGH/LOW (only if not pre-split)
  local SPLIT LOG
  LOG="$OUTPUT.split.log"
  if [[ "$USE_PRESPLIT" != "yes" ]]; then
    printf "hgsplit\t\t"
    SPLIT=$(run_timed "$LOG" \
      "$BUILDPATH/motivo-hgsplit" -i "$GRAPH" -s "$LOW_GRAPH" -l "$HIGH_GRAPH" ${THRESHOLD:+-t "$THRESHOLD"})
    csv_row "split" "" "hgsplit" "$SPLIT" "$LOG"
    printf "%s\n" "$SPLIT"
    emit_step_perf "hgsplit" "0" "$LOG" "$SPLIT" "0" "0" "0"
  else
    echo "[info] Using pre-split basenames: LOW(Gaifman)=$LOW_G  HIGH(HG)=$HIGH_GRAPH" | tee -a "$LOGFILE"
  fi

  # 2) gaifman(LOW) (only if not pre-split; LOW_G must end up being the Gaifman)
  LOG="$OUTPUT.gaifman.log"
  if [[ "$USE_PRESPLIT" != "yes" ]]; then
    printf "gaifman(low)\t"
    # We take LOW_GRAPH hypergraph as input, write Gaifman to the same basename LOW_G
    local GF
    GF=$(run_timed "$LOG" "$BUILDPATH/motivo-gaifman" --input "$LOW_G" --output "$LOW_G" -j "$THREADS")
    csv_row "split" "" "gaifman_low" "$GF" "$LOG"
    printf "%s\n" "$GF"
    emit_step_perf "gaifman_low" "0" "$LOG" "$GF" "$THREADS" "0" "0"
  fi

  # -------- k=1 --------
  LOG="$OUTPUT.buildH1.log"
  B1=$(run_timed "$LOG" "$BUILDPATH/motivo-build" \
        --graph "$LOW_G" --size 1 \
        --colors "$COLORS" \
        --output "$HIGH_TTC" \
        ${SEED:+--seed "$SEED"} --threads "$THREADS")
  csv_row "k1" "1" "build_high" "$B1" "$LOG"
  emit_step_perf "build_high" "1" "$LOG" "$B1" "$THREADS" "0" "0"

  LOG="$OUTPUT.mergeH1.log"
  M1=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
        --output "$HIGH_TTC.1" --compress-threshold "$COMP_THR" \
        "$HIGH_TTC.1.cnt")
  csv_row "k1" "1" "merge_LH" "$M1" "$LOG"
  emit_step_perf "merge_LH" "1" "$LOG" "$M1" "0" "1" "0"

  # ---- NWS(k=1) + IE-merge(k=1): NECESSARI per k>=2 ----
  LOG="$OUTPUT.nwsH1.log"
  N1=$(run_timed "$LOG" "$BUILDPATH/motivo-nws" \
        --graph "$HIGH_GRAPH" --size 1 \
        -i "$HIGH_TTC" --output "$HIGH_TTC" --threads "$THREADS" \
        ${NO_SUBTYPE_PRUNING:+$NO_SUBTYPE_PRUNING})
  csv_row "k1" "1" "nws_high" "$N1" "$LOG"
  emit_step_perf "nws_high" "1" "$LOG" "$N1" "$THREADS" "0" "0"

  LOG="$OUTPUT.nwsMerge1.log"
  I1=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
        -e --output "$GLOBAL_TTC.1.ie" --compress-threshold "$COMP_THR" \
        "$HIGH_TTC.1.ie.cnt")
  csv_row "k1" "1" "merge_ie" "$I1" "$LOG"
  emit_step_perf "merge_ie" "1" "$LOG" "$I1" "0" "1" "0"

  # Prepara GLOBAL_TTC.1.* (non-IE) per coerenza con k>=2
  cp -f "$HIGH_TTC.1.dtz"  "$GLOBAL_TTC.1.dtz"
  cp -f "$HIGH_TTC.1.info" "$GLOBAL_TTC.1.info"
  cp -f "$HIGH_TTC.1.rts"  "$GLOBAL_TTC.1.rts"

  # -------- tabella unificata (stampa umana) --------
  echo -e "k\tH.build\tL.build\tLH.fuse\tLH.merge\tNWS\tIE.merge"
  printf "1\t%s\t%s\t%s\t%s\t\t%s\t%s\n" \
    "$(printf '%.2f' "$B1")" "/" "/" \
    "$(printf '%.2f' "$M1")" "$(printf '%.2f' "$N1")" "$(printf '%.2f' "$I1")"

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

    # HIGH build
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
    emit_step_perf "build_high" "$k" "$LOG" "$HB" "$THREADS" "0" "0"

    # LOW build
    LOG="$OUTPUT.buildL${k}.log"
    LB=$(run_timed "$LOG" "$BUILDPATH/motivo-build" \
          --graph "$LOW_G" --size "$k" \
          --output "$LOW_TTC" \
          -i "$GLOBAL_TTC" --normalize false --threads "$THREADS" \
          ${L_STORE_FLAG:+$L_STORE_FLAG})
    csv_row "kloop" "$k" "build_low" "$LB" "$LOG"
    emit_step_perf "build_low" "$k" "$LOG" "$LB" "$THREADS" "0" "0"

    # Fuse (cnt)
    LOG="$OUTPUT.lowHighFuse${k}.log"
    FH=$(run_timed "$LOG" "$BUILDPATH/motivo-low-high-merge" \
          --low "$LOW_TTC.${k}.cnt" --high "$HIGH_TTC.${k}.cnt" \
          -o "$GLOBAL_TTC.${k}" -c "$LOW_G")
    csv_row "kloop" "$k" "fuse_LH" "$FH" "$LOG"
    emit_step_perf "fuse_LH" "$k" "$LOG" "$FH" "0" "0" "0"

    # Merge (dtz)
    LOG="$OUTPUT.lowHighMerge${k}.log"
    MH=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" --output "$GLOBAL_TTC.${k}" --compress-threshold "$COMP_THR" "$GLOBAL_TTC.${k}.cnt")
    csv_row "kloop" "$k" "merge_LH" "$MH" "$LOG"
    emit_step_perf "merge_LH" "$k" "$LOG" "$MH" "0" "1" "0"

    # NWS su HIGH
    LOG="$OUTPUT.nwsH${k}.log"
    NW=$(run_timed "$LOG" "$BUILDPATH/motivo-nws" \
          --graph "$HIGH_GRAPH" --size "$k" \
          -i "$GLOBAL_TTC" --output "$HIGH_TTC" --threads "$THREADS" \
          ${NO_SUBTYPE_PRUNING:+$NO_SUBTYPE_PRUNING})
    csv_row "kloop" "$k" "nws_high" "$NW" "$LOG"
    emit_step_perf "nws_high" "$k" "$LOG" "$NW" "$THREADS" "0" "0"

    # IE merge
    LOG="$OUTPUT.merge2H${k}.log"
    IE=$(run_timed "$LOG" "$BUILDPATH/motivo-merge" \
          -e --output "$GLOBAL_TTC.${k}.ie" --compress-threshold "$COMP_THR" \
          "$HIGH_TTC.${k}.ie.cnt")
    csv_row "kloop" "$k" "merge_ie" "$IE" "$LOG"
    emit_step_perf "merge_ie" "$k" "$LOG" "$IE" "0" "1" "0"

    printf "%d\t%s\t%s\t%s\t%s\t\t%s\t%s\n" \
      "$k" "$(printf '%.2f' "$HB")" "$(printf '%.2f' "$LB")" \
      "$(printf '%.2f' "$FH")" "$(printf '%.2f' "$MH")" \
      "$(printf '%.2f' "$NW")" "$(printf '%.2f' "$IE")"

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

  # Pass --hypergraph-full only if -g was provided
  local HFULL=()
  if [[ -n "${GRAPH}" ]]; then HFULL=(--hypergraph-full "$GRAPH"); fi

  SMP=$(run_timed "$LOG" "$BUILDPATH/motivo-hyper-sample" \
      --gaifman "$LOW_G" \
      --hypergraph "$HIGH_GRAPH" \
      "${HFULL[@]}" \
      --tables "$GLOBAL_TTC" \
      --nws-high "$GLOBAL_TTC" \
      --size "$MAXSIZE" \
      --num-samples "$SAMPLES" \
      --threads "$THREADS" \
      --group --graphlets --estimate-occurrences --canonicize --spanning-trees --vertices \
      ${SEED:+--seed "$SEED"} \
      --output "$SAMPLE_BASE")
  printf "%s\n" "$SMP"

  emit_step_perf "sample" "$MAXSIZE" "$LOG" "$SMP" "$THREADS" "0" "1"

  echo "Samples are in ${SAMPLE_BASE}.csv:"
  head -6 "${SAMPLE_BASE}.csv" || true
}

# ------------------------- run -------------------------------
mkdir -p "$(dirname -- "$OUTPUT")"
TIMINGS_CSV="${OUTPUT}.timings.csv"
PERF_CSV="${OUTPUT}.perf"
csv_header
perf_header

echo "[$(date)] motivo-hyper start"

if [[ "$BUILD" == "YES" ]]; then build; fi
if [[ "$SAMPLE" == "YES" ]]; then sample; fi

echo "[$(date)] Done."