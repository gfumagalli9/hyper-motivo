#!/usr/bin/env bash
set -euo pipefail

# ------------------------------------------------------------
# Orchestrates a full experiment tranche on one dataset:
# - Converts TXT -> hypergraph bin
# - Splits hypergraph into HIGH/LOW
# - Builds Gaifman on LOW (for hyper pipeline) and FULL (for graph pipeline)
# - Runs hyper and graph pipelines for all T x K x S
# - Collects per-run CSVs and archives all artifacts per (K,T,S)
#   If --delete is given, keep only .log/.perf/.timings.csv/.info and delete the rest.
#   NEW: also records preprocessing timings into $OUTPUT_BASE.preproc.csv
# ------------------------------------------------------------

# ----------------------- configuration -----------------------
BUILDPATH=${BUILDPATH:-../build/bin}
HYPER_PIPE=${HYPER_PIPE:-../scripts/hyper_motivo_mac.sh}
GRAPH_PIPE=${GRAPH_PIPE:-../scripts/motivo_mac.sh}

# ----------------------- helpers -----------------------------

die(){ echo "Error: $*" >&2; exit 1; }

trim() {
  local s="${1-}"
  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"
  printf '%s' "$s"
}
split_list() {
  local s
  s="$(trim "${1-}")"
  s="$(printf '%s' "$s" | tr ',' ' ' | awk '{for(i=1;i<=NF;i++) if($i!="") printf("%s%s",$i,(i<NF?" ":""))}')"
  printf '%s\n' "$s"
}
need_bin(){
  local name="$1"
  local path="${BUILDPATH}/${name}"
  [[ -x "$path" ]] || die "Missing binary: $path"
  echo "$path"
}

# ---------- time(1) detection (GNU) with fallback ----------
TIMECMD=()
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
grep_user_time(){ grep -Eo "User time \(seconds\):[[:space:]]*[0-9.]+$" "$1" | awk '{print $4}' | head -1 | awk '{printf "%.2f\n",$1+0}' || echo "0.00"; }
grep_sys_time(){  grep -Eo "System time \(seconds\):[[:space:]]*[0-9.]+$" "$1" | awk '{print $4}' | head -1 | awk '{printf "%.2f\n",$1+0}' || echo "0.00"; }
grep_elapsed(){   grep -F "Elapsed (wall clock) time" "$1" | tail -1 | awk '{print $NF}' | to_seconds || echo "0.00"; }

# run command; if GNU time -v available parse wall time; else manual timing
run_timed() {
  local log="$1"; shift
  if ((${#TIMECMD[@]})); then
    "${TIMECMD[@]}" "$@" >"$log" 2>&1
    grep_elapsed "$log"
  else
    local t0 t1
    t0=$(date +%s.%N)
    "$@" >"$log" 2>&1
    t1=$(date +%s.%N)
    awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f\n", (b-a)}'
  fi
}

# ---------------- CSV helpers for preprocessing ----------------
PREPROC_CSV=""
preproc_header() {
  echo "date,stage,variant,input,output,threads,walltime,usertime,systemtime,log" >"$PREPROC_CSV"
}
preproc_row() { # stage variant input output threads log
  local stage="$1" variant="$2" inp="$3" out="$4" th="$5" log="$6"
  local ts u s w
  ts="$(date -Iseconds)"
  if ((${#TIMECMD[@]})); then
    u="$(grep_user_time "$log")"
    s="$(grep_sys_time  "$log")"
    w="$(grep_elapsed    "$log")"
  else
    u=""; s=""; w="$(awk -F',' 'END{print $NF}' <<<"")" # ignored; caller passes walltime already
    # fallback: if run_timed was manual, we don't have u/s; we still compute w below
    w="$(awk 'END{print w}' /dev/null 2>/dev/null || true)"
  fi
  # If we came from run_timed, we already know walltime; recompute anyway from log when available.
  w="$(grep_elapsed "$log" || echo "")"
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "$ts" "$stage" "$variant" "$inp" "$out" "$th" "${w:-}" "${u:-}" "${s:-}" "$log" >>"$PREPROC_CSV"
}

# ---------------- Existing helpers kept as-is -----------------
run_hypergraph_build(){
  local in="$1" outbase="$2" bin
  bin="$(need_bin motivo-hypergraph)"
  if "$bin" --input "$in" --output "$outbase"; then return 0; fi
  if "$bin" -i "$in" -o "$outbase"; then return 0; fi
  die "motivo-hypergraph invocation failed for input=$in output=$outbase"
}

# Split input hypergraph into LOW/HIGH basenames
run_hgsplit(){
  local in_base="$1" low_base="$2" high_base="$3" log="$4" bin
  bin="$(need_bin motivo-hgsplit)"
  "$bin" -i "$in_base" -s "$low_base" -l "$high_base" > "$log" 2>&1
}

# Build Gaifman for a given hypergraph base (LOW or FULL)
run_gaifman(){
  local hg_base="$1" out_base="$2" threads="$3" bin
  bin="$(need_bin motivo-gaifman)"
  "$bin" --input "$hg_base" --output "$out_base" --stream > "${out_base}.gaif.log" 2>&1
}

run_dedup(){
  local bin_debin="${BUILDPATH}/motivo-hgdedup"
  local bin_txt="${BUILDPATH}/motivo-dedup"
  if [[ -x "$bin_debin" ]]; then
    if "$bin_debin" "$HG_BIN_BASE" "$HG_DEDUP_BIN_BASE" \
       || "$bin_debin" -i "$HG_BIN_BASE" -o "$HG_DEDUP_BIN_BASE"; then
      return 0
    fi
    die "motivo-hgdedup failed"
  elif [[ -x "$bin_txt" ]]; then
    HG_DEDUP_TXT="${HG_DEDUP_BIN_BASE}.txt"
    if "$bin_txt" --input "$HG_TXT" --output "$HG_DEDUP_TXT" 2>/dev/null \
       || "$bin_txt" -i "$HG_TXT" -o "$HG_DEDUP_TXT" 2>/dev/null ; then
      run_hypergraph_build "$HG_DEDUP_TXT" "$HG_DEDUP_BIN_BASE"
      return 0
    fi
    die "motivo-dedup failed on text input"
  else
    die "No dedup binary found (motivo-hgdedup or motivo-dedup)"
  fi
}

pick_graph_pipe(){
  if [[ -x "$GRAPH_PIPE" ]]; then
    echo "$GRAPH_PIPE"
  elif [[ -x ../scripts/motivo.sh ]]; then
    echo "../scripts/motivo.sh"
  else
    die "Graph pipeline script not found (GRAPH_PIPE=$GRAPH_PIPE)"
  fi
}
max_of_list(){
  awk 'BEGIN{m=""} {for(i=1;i<=NF;i++){if(m==""||$i+0>m)m=$i+0}} END{if(m=="") m=1; print m}'
}
base_noext(){
  local p="$1"; local b; b="$(basename -- "$p")"; echo "${b%.*}"
}

# Prefer .perf (CSV per-step); fallback to .timings.csv
copy_stepcsv_if_exists(){
  local prefix="$1" dest="$2"
  if [[ -f "${prefix}.perf" ]]; then
    cp -f "${prefix}.perf" "$dest"
    echo "[info] saved step-CSV -> $dest"
  elif [[ -f "${prefix}.timings.csv" ]]; then
    cp -f "${prefix}.timings.csv" "$dest"
    echo "[info] saved timings -> $dest"
  fi
}

# NEW: delete-mode flag
DELETE_MODE="no"

# Archive artifacts for a given OUT_* prefix into RUN_DIR subfolders.
# If DELETE_MODE=yes -> move ONLY logs/perf/csv/info and delete everything else for that prefix.
archive_run_artifacts() {
  local run_dir="$1" kind="$2" variant="$3" prefix="$4"
  local dest="$run_dir/$kind/$variant"
  mkdir -p "$dest"
  shopt -s nullglob
  if [[ "$DELETE_MODE" == "yes" ]]; then
    for f in "${prefix}.perf" "${prefix}.timings.csv" "${prefix}"*.log "${prefix}"*.info; do
      [[ -e "$f" ]] && mv -f "$f" "$dest/"
    done
    for p in \
      "${prefix}"* \
      "${prefix}-High"* \
      "${prefix}-Low"* \
      "${prefix}-HighTTC"* \
      "${prefix}-LowTTC"* \
    ; do
      [[ -e "$p" ]] && rm -rf "$p"
    done
  else
    for p in \
      "${prefix}"* \
      "${prefix}-High"* \
      "${prefix}-Low"* \
      "${prefix}-HighTTC"* \
      "${prefix}-LowTTC"* \
    ; do
      [[ -e "$p" ]] && mv -f "$p" "$dest/"
    done
  fi
  shopt -u nullglob
}

# Copy available preprocessing logs into the per-run dir (do not move here).
copy_preproc_logs() {
  local run_dir="$1"
  mkdir -p "$run_dir/preproc"
  for f in \
      "${GAIF_FULL_ORIG_BASE}.gaif.log" \
      "${GAIF_FULL_DEDUP_BASE:-}.gaif.log" \
      "${GAIF_LOW_ORIG_BASE}.gaif.log" \
      "${GAIF_LOW_DEDUP_BASE:-}.gaif.log" \
      "${OUTPUT_BASE}.convert.log" \
      "${OUTPUT_BASE}.split_orig.log" \
      "${OUTPUT_BASE}.split_dedup.log" \
      "${OUTPUT_BASE}.dedup.log" \
      "${PREPROC_CSV}" \
  ; do
    [[ -f "$f" ]] && cp -f "$f" "$run_dir/preproc/"
  done
}

# Final cleanup for preprocessing artifacts if DELETE_MODE=yes
final_cleanup_preproc() {
  [[ "$DELETE_MODE" == "yes" ]] || return 0
  shopt -s nullglob
  rm -f "${HG_BIN_BASE}".hmeta "${HG_BIN_BASE}".hbin "${HG_BIN_BASE}".hidx || true
  rm -f "${SPLIT_LOW_ORIG_BASE}".* "${SPLIT_HIGH_ORIG_BASE}".* || true
  rm -f "${GAIF_FULL_ORIG_BASE}".* "${GAIF_LOW_ORIG_BASE}".* || true
  if [[ "${DO_DEDUP}" == "yes" ]]; then
    rm -f "${HG_DEDUP_BIN_BASE}".hmeta "${HG_DEDUP_BIN_BASE}".hbin "${HG_DEDUP_BIN_BASE}".hidx || true
    rm -f "${SPLIT_LOW_DEDUP_BASE}".* "${SPLIT_HIGH_DEDUP_BASE}".* || true
    rm -f "${GAIF_FULL_DEDUP_BASE}".* "${GAIF_LOW_DEDUP_BASE}".* || true
  fi
  # keep logs + ${PREPROC_CSV} (already copied to run_dir/preproc); remove root copies to declutter
  rm -f \
    "${OUTPUT_BASE}.convert.log" \
    "${OUTPUT_BASE}.dedup.log" \
    "${OUTPUT_BASE}.split_orig.log" \
    "${OUTPUT_BASE}.split_dedup.log" \
    "${GAIF_FULL_ORIG_BASE}.gaif.log" \
    "${GAIF_LOW_ORIG_BASE}.gaif.log" \
    "${GAIF_FULL_DEDUP_BASE:-}.gaif.log" \
    "${GAIF_LOW_DEDUP_BASE:-}.gaif.log" \
    || true
  shopt -u nullglob
  echo "[cleanup] removed preprocessing artifacts (delete mode)"
}

# ----------------------- parse args --------------------------
THREADS_LIST=""
K_SINGLE=""
K_LIST=""
SAMPLES_LIST=""
HG_TXT=""
DO_DEDUP="no"
OUTPUT_BASE=""
RESULTS_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --threads)         THREADS_LIST="$(split_list "$(trim "${2:-}")")"; shift 2 ;;
    -k|--k)            K_SINGLE="$(trim "${2:-}")"; shift 2 ;;
    --k-list)          K_LIST="$(split_list "$(trim "${2:-}")")"; shift 2 ;;
    --samples)         SAMPLES_LIST="$(split_list "$(trim "${2:-}")")"; shift 2 ;;
    --hg)              HG_TXT="$(trim "${2:-}")"; shift 2 ;;
    --deduplicate)     DO_DEDUP="yes"; shift ;;
    --delete)          DELETE_MODE="yes"; shift ;;
    -o|--output)       OUTPUT_BASE="$(trim "${2:-}")"; shift 2 ;;
    -R|--results)      RESULTS_DIR="$(trim "${2:-}")"; shift 2 ;;
    -h|--help)
      cat <<EOF
Usage:
  $0 --threads "1,8" -k 3 --samples "1000 100000" --hg path/to/hyper.txt \\
     [--deduplicate] [--delete] --output out/basename --results results/
EOF
      exit 0 ;;
    *) die "Unknown option: $1" ;;
  esac
done

[[ -n "$THREADS_LIST" ]]   || die "Missing --threads"
if [[ -z "$K_LIST" ]]; then
  [[ -n "$K_SINGLE" ]]     || die "Missing -k/--k or --k-list"
  K_LIST="$K_SINGLE"
fi
[[ -n "$SAMPLES_LIST" ]]   || die "Missing --samples"
[[ -n "$HG_TXT" ]]         || die "Missing --hg (path to hypergraph .txt)"
[[ -n "$OUTPUT_BASE" ]]    || die "Missing --output"
[[ -n "$RESULTS_DIR" ]]    || die "Missing --results"

[[ -x "$HYPER_PIPE" ]] || die "Hyper pipeline not found/executable: $HYPER_PIPE"
GRAPH_PIPE="$(pick_graph_pipe)"

mkdir -p "$RESULTS_DIR"
mkdir -p "$(dirname -- "$OUTPUT_BASE")"

# ----------------------- pre-processing ----------------------
HG_NAME_NOEXT="$(base_noext "$HG_TXT")"
MAXT="$(echo "$THREADS_LIST" | max_of_list)"

PREPROC_CSV="${OUTPUT_BASE}.preproc.csv"
preproc_header

# 1) TXT -> BIN  (timed)
HG_BIN_BASE="${OUTPUT_BASE}.hg"
echo "[step] hypergraph(txt->bin)   $HG_TXT  ->  ${HG_BIN_BASE}.*"
{
  bin="$(need_bin motivo-hypergraph)"
  LOG="${OUTPUT_BASE}.convert.log"
  secs=""
  if ! secs="$(run_timed "$LOG" "$bin" --input "$HG_TXT" --output "$HG_BIN_BASE")"; then
    # fallback to short flags
    : > "$LOG"
    secs="$(run_timed "$LOG" "$bin" -i "$HG_TXT" -o "$HG_BIN_BASE")"
  fi
  preproc_row "build_hypergraph" "orig" "$HG_TXT" "$HG_BIN_BASE" "1" "$LOG"
} || true

# 2) Split original -> LOW/HIGH (timed)
SPLIT_LOW_ORIG_BASE="${OUTPUT_BASE}.low"
SPLIT_HIGH_ORIG_BASE="${OUTPUT_BASE}.high"
echo "[step] split(original)        ${HG_BIN_BASE}  ->  ${SPLIT_LOW_ORIG_BASE}.*, ${SPLIT_HIGH_ORIG_BASE}.*"
{
  bin="$(need_bin motivo-hgsplit)"
  LOG="${OUTPUT_BASE}.split_orig.log"
  secs="$(run_timed "$LOG" "$bin" -i "$HG_BIN_BASE" -s "$SPLIT_LOW_ORIG_BASE" -l "$SPLIT_HIGH_ORIG_BASE")"
  preproc_row "split" "orig" "$HG_BIN_BASE" "${SPLIT_LOW_ORIG_BASE}|${SPLIT_HIGH_ORIG_BASE}" "1" "$LOG"
} || true

# 3) Gaifman FULL(original)  -> per graph pipeline (timed)
GAIF_FULL_ORIG_BASE="${OUTPUT_BASE}.gaifman_full"
echo "[step] gaifman(full,orig)     ${HG_BIN_BASE}  ->  ${GAIF_FULL_ORIG_BASE}.*"
{
  bin="$(need_bin motivo-gaifman)"
  LOG="${GAIF_FULL_ORIG_BASE}.gaif.log"
  secs="$(run_timed "$LOG" "$bin" --input "$HG_BIN_BASE" --output "$GAIF_FULL_ORIG_BASE" --stream)"
  preproc_row "gaifman_full" "orig" "$HG_BIN_BASE" "$GAIF_FULL_ORIG_BASE" "$MAXT" "$LOG"
} || true

# 4) Gaifman LOW(original)   -> per hyper pipeline (timed, in-place basename of LOW split)
GAIF_LOW_ORIG_BASE="${SPLIT_LOW_ORIG_BASE}"
echo "[step] gaifman(low,orig)      ${SPLIT_LOW_ORIG_BASE}  ->  ${GAIF_LOW_ORIG_BASE}.*"
{
  bin="$(need_bin motivo-gaifman)"
  LOG="${GAIF_LOW_ORIG_BASE}.gaif.log"
  secs="$(run_timed "$LOG" "$bin" --input "$SPLIT_LOW_ORIG_BASE" --output "$SPLIT_LOW_ORIG_BASE" -j "$MAXT")"
  preproc_row "gaifman_low" "orig" "$SPLIT_LOW_ORIG_BASE" "$GAIF_LOW_ORIG_BASE" "$MAXT" "$LOG"
} || true

# ----- Optional dedup variant -----
HG_DEDUP_BIN_BASE="${OUTPUT_BASE}.hg_dedup"
SPLIT_LOW_DEDUP_BASE="${OUTPUT_BASE}.low_dedup"
SPLIT_HIGH_DEDUP_BASE="${OUTPUT_BASE}.high_dedup"
GAIF_LOW_DEDUP_BASE="${SPLIT_LOW_DEDUP_BASE}"
GAIF_FULL_DEDUP_BASE="${OUTPUT_BASE}.gaifman_full_dedup"

if [[ "$DO_DEDUP" == "yes" ]]; then
  echo "[step] deduplicate            ${HG_BIN_BASE}  ->  ${HG_DEDUP_BIN_BASE}.*"
  {
    LOG="${OUTPUT_BASE}.dedup.log"
    if [[ -x "${BUILDPATH}/motivo-hgdedup" ]]; then
      secs="$(run_timed "$LOG" "${BUILDPATH}/motivo-hgdedup" -i "$HG_BIN_BASE" -o "$HG_DEDUP_BIN_BASE")"
      preproc_row "dedup" "dedup" "$HG_BIN_BASE" "$HG_DEDUP_BIN_BASE" "1" "$LOG"
    elif [[ -x "${BUILDPATH}/motivo-dedup" ]]; then
      # dedup on TXT then rebuild BIN (two rows)
      HG_DEDUP_TXT="${HG_DEDUP_BIN_BASE}.txt"
      secs="$(run_timed "$LOG" "${BUILDPATH}/motivo-dedup" -i "$HG_TXT" -o "$HG_DEDUP_TXT")"
      preproc_row "dedup_txt" "dedup" "$HG_TXT" "$HG_DEDUP_TXT" "1" "$LOG"
      # rebuild BIN (timed)
      LOG="${OUTPUT_BASE}.convert_dedup.log"
      bin="$(need_bin motivo-hypergraph)"
      if ! secs="$(run_timed "$LOG" "$bin" --input "$HG_DEDUP_TXT" --output "$HG_DEDUP_BIN_BASE")"; then
        : > "$LOG"
        secs="$(run_timed "$LOG" "$bin" -i "$HG_DEDUP_TXT" -o "$HG_DEDUP_BIN_BASE")"
      fi
      preproc_row "build_hypergraph" "dedup" "$HG_DEDUP_TXT" "$HG_DEDUP_BIN_BASE" "1" "$LOG"
    else
      die "No dedup binary found (motivo-hgdedup or motivo-dedup)"
    fi
  } || true

  echo "[step] split(dedup)           ${HG_DEDUP_BIN_BASE}  ->  ${SPLIT_LOW_DEDUP_BASE}.*, ${SPLIT_HIGH_DEDUP_BASE}.*"
  {
    bin="$(need_bin motivo-hgsplit)"
    LOG="${OUTPUT_BASE}.split_dedup.log"
    secs="$(run_timed "$LOG" "$bin" -i "$HG_DEDUP_BIN_BASE" -s "$SPLIT_LOW_DEDUP_BASE" -l "$SPLIT_HIGH_DEDUP_BASE")"
    preproc_row "split" "dedup" "$HG_DEDUP_BIN_BASE" "${SPLIT_LOW_DEDUP_BASE}|${SPLIT_HIGH_DEDUP_BASE}" "1" "$LOG"
  } || true

  echo "[step] gaifman(full,dedup)    ${HG_DEDUP_BIN_BASE}  ->  ${GAIF_FULL_DEDUP_BASE}.*"
  {
    bin="$(need_bin motivo-gaifman)"
    LOG="${GAIF_FULL_DEDUP_BASE}.gaif.log"
    secs="$(run_timed "$LOG" "$bin" --input "$HG_DEDUP_BIN_BASE" --output "$GAIF_FULL_DEDUP_BASE" -j "$MAXT")"
    preproc_row "gaifman_full" "dedup" "$HG_DEDUP_BIN_BASE" "$GAIF_FULL_DEDUP_BASE" "$MAXT" "$LOG"
  } || true

  echo "[step] gaifman(low,dedup)     ${SPLIT_LOW_DEDUP_BASE}  ->  ${GAIF_LOW_DEDUP_BASE}.*"
  {
    bin="$(need_bin motivo-gaifman)"
    LOG="${GAIF_LOW_DEDUP_BASE}.gaif.log"
    secs="$(run_timed "$LOG" "$bin" --input "$SPLIT_LOW_DEDUP_BASE" --output "$SPLIT_LOW_DEDUP_BASE" -j "$MAXT")"
    preproc_row "gaifman_low" "dedup" "$SPLIT_LOW_DEDUP_BASE" "$GAIF_LOW_DEDUP_BASE" "$MAXT" "$LOG"
  } || true
fi

# Salva anche una copia "globale" dei tempi preproc nella cartella risultati
cp -f "$PREPROC_CSV" "${RESULTS_DIR}/${HG_NAME_NOEXT}_preproc.csv"

# ----------------------- sweeps T x K x S --------------------
for T in $THREADS_LIST; do
  for K in $K_LIST; do
    for S in $SAMPLES_LIST; do

      RUN_TAG="${HG_NAME_NOEXT}_K${K}_T${T}_S${S}"
      RUN_DIR="${RESULTS_DIR}/${RUN_TAG}"
      mkdir -p "$RUN_DIR"

      # ---------- HYPER (original) ----------
      OUT_HYP_ORIG="${OUTPUT_BASE}.hyper.K${K}.T${T}.S${S}"
      echo "[run] hyper(original) K=$K T=$T S=$S -> $OUT_HYP_ORIG"
      bash "$HYPER_PIPE" --build --sample \
        -H "${SPLIT_HIGH_ORIG_BASE}" \
        -L "${GAIF_LOW_ORIG_BASE}" \
        -g "${HG_BIN_BASE}" \
        -k "$K" \
        -S "$S" \
        -t "$T" \
        -o "${OUT_HYP_ORIG}" \
        --seed 42 \
        > "${OUT_HYP_ORIG}.driver.log" 2>&1 || true

      copy_stepcsv_if_exists "${OUT_HYP_ORIG}" \
        "${RESULTS_DIR}/${HG_NAME_NOEXT}_hyper_K${K}_T${T}_S${S}.csv"
      archive_run_artifacts "$RUN_DIR" "hyper" "orig" "$OUT_HYP_ORIG"

      # ---------- HYPER (deduplicated) ----------
      if [[ "$DO_DEDUP" == "yes" ]]; then
        OUT_HYP_DEDUP="${OUTPUT_BASE}.hyperDedup.K${K}.T${T}.S${S}"
        echo "[run] hyper(dedup)    K=$K T=$T S=$S -> $OUT_HYP_DEDUP"
        bash "$HYPER_PIPE" --build --sample \
          -H "${SPLIT_HIGH_DEDUP_BASE}" \
          -L "${GAIF_LOW_DEDUP_BASE}" \
          -g "${HG_DEDUP_BIN_BASE}" \
          -k "$K" \
          -S "$S" \
          -t "$T" \
          -o "${OUT_HYP_DEDUP}" \
          --seed 42 \
          > "${OUT_HYP_DEDUP}.driver.log" 2>&1 || true

        copy_stepcsv_if_exists "${OUT_HYP_DEDUP}" \
          "${RESULTS_DIR}/${HG_NAME_NOEXT}_hyper_dedup_K${K}_T${T}_S${S}.csv"
        archive_run_artifacts "$RUN_DIR" "hyper" "dedup" "$OUT_HYP_DEDUP"
      fi

      # ---------- GRAPH (Gaifman FULL - original) ----------
      OUT_G_ORIG="${OUTPUT_BASE}.graph.K${K}.T${T}.S${S}"
      echo "[run] graph(original) K=$K T=$T S=$S -> $OUT_G_ORIG"
      bash "$GRAPH_PIPE" \
        -g "${GAIF_FULL_ORIG_BASE}" \
        -k "$K" \
        -o "$OUT_G_ORIG" \
        -s "$S" \
        -t "$T" \
        -H "${HG_BIN_BASE}" \
        --seed 42 \
        > "${OUT_G_ORIG}.driver.log" 2>&1 || true

      if [[ -f "${OUT_G_ORIG}.perf" || -f "${OUT_G_ORIG}.timings.csv" ]]; then
        copy_stepcsv_if_exists "${OUT_G_ORIG}" \
          "${RESULTS_DIR}/${HG_NAME_NOEXT}_gaifman_K${K}_T${T}_S${S}.csv"
      fi
      archive_run_artifacts "$RUN_DIR" "graph" "orig" "$OUT_G_ORIG"

      # ---------- GRAPH (Gaifman FULL - deduplicated) ----------
      if [[ "$DO_DEDUP" == "yes" ]]; then
        OUT_G_DEDUP="${OUTPUT_BASE}.graphDedup.K${K}.T${T}.S${S}"
        echo "[run] graph(dedup)    K=$K T=$T S=$S -> $OUT_G_DEDUP"
        bash "$GRAPH_PIPE" \
          -g "${GAIF_FULL_DEDUP_BASE}" \
          -k "$K" \
          -o "$OUT_G_DEDUP" \
          -s "$S" \
          -t "$T" \
          -H "${HG_DEDUP_BIN_BASE}" \
          --seed 42 \
          > "${OUT_G_DEDUP}.driver.log" 2>&1 || true

        if [[ -f "${OUT_G_DEDUP}.perf" || -f "${OUT_G_DEDUP}.timings.csv" ]]; then
          copy_stepcsv_if_exists "${OUT_G_DEDUP}" \
            "${RESULTS_DIR}/${HG_NAME_NOEXT}_gaifman_dedup_K${K}_T${T}_S${S}.csv"
        fi
        archive_run_artifacts "$RUN_DIR" "graph" "dedup" "$OUT_G_DEDUP"
      fi

      # ---------- add preproc logs snapshot + CSV ----------
      copy_preproc_logs "$RUN_DIR"

      echo "[archived] ${RUN_DIR}"
    done
  done
done

# optional: purge preprocessing artifacts if requested
final_cleanup_preproc

echo "[done] All timings are in: $RESULTS_DIR"