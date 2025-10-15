#!/usr/bin/env bash
set -euo pipefail

# Orchestrates a full experiment tranche on one dataset:
# - Builds Gaifman for original and (optionally) deduplicated hypergraph
# - Runs hyper and graph pipelines for all T x K x S
# - Collects timing CSVs into --results with consistent names
# - After EACH (K,T,S), archives all artifacts/logs under results/<DATASET>_K<K>_T<T>_S<S>/
#
# Example:
#   ./run_experiments.sh \
#     --threads "1,8" \
#     -k 3 \
#     --samples "1000 100000" \
#     --hg data/hyperedges_mathoverflow.txt \
#     --deduplicate \
#     --output out/mathoverflow \
#     --results results/

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
run_hypergraph_build(){
  local in="$1" outbase="$2" bin
  bin="$(need_bin motivo-hypergraph)"
  if "$bin" --input "$in" --output "$outbase"; then return 0; fi
  if "$bin" -i "$in" -o "$outbase"; then return 0; fi
  die "motivo-hypergraph invocation failed for input=$in output=$outbase"
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
run_gaifman(){
  local hg_base="$1" out_base="$2" threads="$3" bin
  bin="$(need_bin motivo-gaifman)"
  if ! "$bin" --input "$hg_base" --output "$out_base" -j "$threads" \
        > "${out_base}.gaif.log" 2>&1 ; then
    die "motivo-gaifman failed for $hg_base"
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

# Archive all artifacts for a given OUT_* prefix into RUN_DIR subfolders.
archive_run_artifacts() {
  local run_dir="$1" kind="$2" variant="$3" prefix="$4"
  mkdir -p "$run_dir/$kind/$variant"
  shopt -s nullglob
  local p
  for p in \
      "${prefix}"* \
      "${prefix}-High"* \
      "${prefix}-Low"* \
      "${prefix}-HighTTC"* \
      "${prefix}-LowTTC"* \
  ; do
    [[ -e "$p" ]] && mv -f "$p" "$run_dir/$kind/$variant/"
  done
  shopt -u nullglob
}

# Copy available preprocessing logs into the per-run dir (do not move).
copy_preproc_logs() {
  local run_dir="$1"
  mkdir -p "$run_dir/preproc"
  for f in \
      "${GAIF_ORIG_BASE}.gaif.log" \
      "${GAIF_DEDUP_BASE:-}.gaif.log" \
      "${OUTPUT_BASE}.convert.log" \
      "${OUTPUT_BASE}.dedup.log" \
  ; do
    [[ -f "$f" ]] && cp -f "$f" "$run_dir/preproc/"
  done
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
    -o|--output)       OUTPUT_BASE="$(trim "${2:-}")"; shift 2 ;;
    -R|--results)      RESULTS_DIR="$(trim "${2:-}")"; shift 2 ;;
    -h|--help)
      cat <<EOF
Usage:
  $0 --threads "1,8" -k 3 --samples "1000 100000" --hg path/to/hyper.txt \\
     [--deduplicate] --output out/basename --results results/
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
HG_BIN_BASE="${OUTPUT_BASE}.hg"
echo "[step] hypergraph(txt->bin)   $HG_TXT  ->  ${HG_BIN_BASE}.*"
run_hypergraph_build "$HG_TXT" "$HG_BIN_BASE" > "${OUTPUT_BASE}.convert.log" 2>&1 || true

MAXT="$(echo "$THREADS_LIST" | max_of_list)"
GAIF_ORIG_BASE="${OUTPUT_BASE}.gaifman"
echo "[step] gaifman(original)      ${HG_BIN_BASE}  ->  ${GAIF_ORIG_BASE}.*"
run_gaifman "$HG_BIN_BASE" "$GAIF_ORIG_BASE" "$MAXT"

HG_DEDUP_BIN_BASE="${OUTPUT_BASE}.hg_dedup"
GAIF_DEDUP_BASE="${OUTPUT_BASE}.gaifman_dedup"
if [[ "$DO_DEDUP" == "yes" ]]; then
  echo "[step] deduplicate            ${HG_BIN_BASE}  ->  ${HG_DEDUP_BIN_BASE}.*"
  run_dedup > "${OUTPUT_BASE}.dedup.log" 2>&1 || true
  echo "[step] gaifman(deduplicated)  ${HG_DEDUP_BIN_BASE}  ->  ${GAIF_DEDUP_BASE}.*"
  run_gaifman "$HG_DEDUP_BIN_BASE" "$GAIF_DEDUP_BASE" "$MAXT"
fi

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
        -g "${HG_BIN_BASE}" \
        -k "$K" \
        -S "$S" \
        -t "$T" \
        -o "$OUT_HYP_ORIG" \
        > "${OUT_HYP_ORIG}.driver.log" 2>&1 || true

      copy_stepcsv_if_exists "${OUT_HYP_ORIG}" \
        "${RESULTS_DIR}/${HG_NAME_NOEXT}_hyper_K${K}_T${T}_S${S}.csv"
      archive_run_artifacts "$RUN_DIR" "hyper" "orig" "$OUT_HYP_ORIG"

      # ---------- HYPER (deduplicated) ----------
      if [[ "$DO_DEDUP" == "yes" ]]; then
        OUT_HYP_DEDUP="${OUTPUT_BASE}.hyperDedup.K${K}.T${T}.S${S}"
        echo "[run] hyper(dedup)    K=$K T=$T S=$S -> $OUT_HYP_DEDUP"
        bash "$HYPER_PIPE" --build --sample \
          -g "${HG_DEDUP_BIN_BASE}" \
          -k "$K" \
          -S "$S" \
          -t "$T" \
          -o "$OUT_HYP_DEDUP" \
          > "${OUT_HYP_DEDUP}.driver.log" 2>&1 || true

        copy_stepcsv_if_exists "${OUT_HYP_DEDUP}" \
          "${RESULTS_DIR}/${HG_NAME_NOEXT}_hyper_dedup_K${K}_T${T}_S${S}.csv"
        archive_run_artifacts "$RUN_DIR" "hyper" "dedup" "$OUT_HYP_DEDUP"
      fi

      # ---------- GRAPH (original Gaifman) ----------
      OUT_G_ORIG="${OUTPUT_BASE}.graph.K${K}.T${T}.S${S}"
      echo "[run] graph(original) K=$K T=$T S=$S -> $OUT_G_ORIG"
      bash "$GRAPH_PIPE" \
        -g "${GAIF_ORIG_BASE}" \
        -k "$K" \
        -o "$OUT_G_ORIG" \
        -s "$S" \
        -t "$T" \
        > "${OUT_G_ORIG}.driver.log" 2>&1 || true

      if [[ -f "${OUT_G_ORIG}.perf" || -f "${OUT_G_ORIG}.timings.csv" ]]; then
        copy_stepcsv_if_exists "${OUT_G_ORIG}" \
          "${RESULTS_DIR}/${HG_NAME_NOEXT}_gaifman_K${K}_T${T}_S${S}.csv"
      fi
      archive_run_artifacts "$RUN_DIR" "graph" "orig" "$OUT_G_ORIG"

      # ---------- GRAPH (deduplicated Gaifman) ----------
      if [[ "$DO_DEDUP" == "yes" ]]; then
        OUT_G_DEDUP="${OUTPUT_BASE}.graphDedup.K${K}.T${T}.S${S}"
        echo "[run] graph(dedup)    K=$K T=$T S=$S -> $OUT_G_DEDUP"
        bash "$GRAPH_PIPE" \
          -g "${GAIF_DEDUP_BASE}" \
          -k "$K" \
          -o "$OUT_G_DEDUP" \
          -s "$S" \
          -t "$T" \
          > "${OUT_G_DEDUP}.driver.log" 2>&1 || true

        if [[ -f "${OUT_G_DEDUP}.perf" || -f "${OUT_G_DEDUP}.timings.csv" ]]; then
          copy_stepcsv_if_exists "${OUT_G_DEDUP}" \
            "${RESULTS_DIR}/${HG_NAME_NOEXT}_gaifman_dedup_K${K}_T${T}_S${S}.csv"
        fi
        archive_run_artifacts "$RUN_DIR" "graph" "dedup" "$OUT_G_DEDUP"
      fi

      # ---------- add preproc logs snapshot ----------
      copy_preproc_logs "$RUN_DIR"

      echo "[archived] ${RUN_DIR}"
    done
  done
done

echo "[done] All timings are in: $RESULTS_DIR"