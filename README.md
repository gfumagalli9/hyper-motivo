# HyperMotivo

This README shows how to:

1. install the required libraries  
2. compile HyperMotivo  
3. run the **`run_experiments.sh`** on a dataset and check the expected outputs

## 0. Datasets

To reproduce our experiments, first build the hypergraph datasets with the helper scripts below.  
Each command produces a plain-text hypergraph where **each line is one hyperedge** (a set of integer vertex IDs).

### 0.1 StackExchange – Data Science

```bash
python3 datascience.py   --site datascience.stackexchange.com   --min-tag-size 1   --edges-txt ds_edges.txt
```

This downloads the StackExchange dump for *datascience.stackexchange.com*, builds a hypergraph where vertices are questions and hyperedges are tags, and writes:

- `ds_edges.txt` — one hyperedge per line, containing the integer IDs of questions sharing the same tag.

### 0.2 Patents – CPC (2024–2025)

```bash
python3 cpc.py   --out cpc_group_2024_2025.txt   --granularity group   --year-from 2024 --year-to 2025
```

This downloads the PatentsView CPC tables, selects patents granted between 2024 and 2025, groups them by CPC **group** code, and writes:

- `cpc_group_2024_2025.txt` — one CPC group per line, containing the integer IDs of patents assigned to that group.

### 0.3 arXiv – Categories (2024)

```bash
python3 arxiv.py   --from 2024-01-01 --until 2025-01-01   --out arxiv_cats_2024.txt
```

This queries arXiv via OAI-PMH, collects all articles in the given date range, builds a hypergraph where vertices are articles and hyperedges are arXiv categories, and writes:

- `arxiv_cats_2024.txt` — one category per line, containing the integer IDs of articles assigned to that category.

The `.txt` files produced as `--out` (and `--edges-txt` for the StackExchange dataset) are already in the ASCII hypergraph format expected by the HyperMotivo pipeline and thus can be used without any additional preprocessing.

---

## 1. Prerequisites

Tested on Linux x86_64 (Debian/Ubuntu-like).

### System packages

```bash
sudo apt-get update
sudo apt-get install -y     build-essential cmake git     libsparsehash-dev libnauty2-dev liblz4-dev     libgoogle-perftools-dev  # optional (tcmalloc)
```

Requirements in short:

- C++17 compiler 
- CMake ≥ 3.12
- Libraries:
  - sparsehash (libsparsehash-dev)
  - nauty (libnauty2-dev)
  - lz4 (liblz4-dev)
  - optional: tcmalloc (libgoogle-perftools-dev)
---

## 2. Build instructions

From the **repository root** (the directory that contains `CMakeLists.txt`):

```bash
mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)"
```

Binaries will be in:

```text
build/bin/
```

### Quick test of the build (optional)

From `build/`:

```bash
ctest
```

---

## 3. Minimal end-to-end run on a toy hypergraph

This section runs the **entire hypergraph pipeline** on a tiny example, to confirm everything works.

### 3.1 Create a tiny hypergraph (ASCII format)

From the **repository root**:

```bash
mkdir -p data

cat > data/toy_hg.txt <<EOF
0 1 2
2 3 4
1 2 4
EOF
```

Each line is a hyperedge; vertex IDs are non-negative integers.  
Any non-numeric separator (space, comma, tab) is accepted.

### 3.2 Convert the toy hypergraph to Motivo binary format

Now convert the ASCII hypergraph to the Motivo hypergraph binary format.  
This step produces `data/toy_hg.hmeta`, `data/toy_hg.hef`, `data/toy_hg.hvd`, `data/toy_hg.vhef`, `data/toy_hg.vhed`.

From the **repository root**:

```bash
build/bin/motivo-hypergraph   --input data/toy_hg.txt   --output data/toy_hg
```

Here `data/toy_hg` is the **basename**; the tool will write:

- `data/toy_hg.hmeta`
- `data/toy_hg.hef`
- `data/toy_hg.hvd`
- `data/toy_hg.vhef`
- `data/toy_hg.vhed`

These files are exactly what the hypergraph pipeline expects.

### 3.3 Run the hypergraph wrapper (`hyper_motivo_mac.sh`)

Now run the pipeline using the wrapper script.

From the **repository root**:

```bash
BUILDPATH=build/bin ./scripts/hyper_motivo_mac.sh --build --sample   -g data/toy_hg      \  # basename of the hypergraph (ASCII or bin)
  -k 3                \  # k-hypergraphlet size
  -o runs/toy_k3      \  # basename for all outputs
  -t 2                \  # number of threads
  -S 10000               # number of samples
```

This command:

- builds the binary hypergraph representation
- performs LOW/HIGH splitting
- constructs Gaifman on LOW
- builds counter tables
- runs the sampler for `k = 3` with 10k samples

### 3.4 What you should see

Under `runs/` (from the repo root), you should find files with prefix `runs/toy_k3*`, including for example:

- `runs/toy_k3.timings.csv`  
  Step-by-step wall-clock timings for the pipeline.

- one or more `runs/toy_k3.sample3.csv` (or similar)  
  CSV with sampled 3-hypergraphlets and associated statistics.

If these files are created and the command exits without error, the HyperMotivo pipeline is correctly installed and working.

---

## 4. Running `scripts/run_experiments.sh` on a dataset

This section shows how to run the **full experimental pipeline** on a dataset using the `run_experiments.sh` orchestrator.

This script:

- converts ASCII hypergraph → binary format  
- performs LOW/HIGH splitting  
- builds Gaifman graphs  
- runs both:
  - the **hypergraph pipeline** (HyperMotivo), and
  - the **graph pipeline** (Motivo on Gaifman)  
- collects timings / performance metrics into CSV files
- archives all artifacts per combination of parameters (K, threads, samples)

### 4.1 Input dataset

We use the same tiny toy dataset as above, stored in:

```text
data/toy_hg.txt
```

Format: one hyperedge per line, vertex IDs separated by spaces.

You can replace `data/toy_hg.txt` with any larger dataset (using the same format) without changing the commands below, only the filename.

### 4.2 Minimal `run_experiments.sh` invocation

From the **repository root**:

```bash
mkdir -p runs results

BUILDPATH=build/bin ./scripts/run_experiments.sh   --threads "2" -k 3 --samples "10000" --hg data/toy_hg.txt --output runs/toy --results results/toy_experiment
```

Meaning of the options:

- `BUILDPATH=build/bin`  
  tells the script where compiled binaries live.

- `--threads "2"`  
  run the pipelines with 2 threads (a list: you could also use `"1 4 8"`).

- `-k 3`  
  run for `k = 3` (size of hypergraphlets).  
  Alternatively, `--k-list "3 4 5"` for multiple values.

- `--samples "10000"`  
  list of sample sizes; here a single value S = 10000.

- `--hg data/toy_hg.txt`  
  path to the **ASCII** hypergraph file.

- `--output runs/toy`  
  base name for all intermediate and pipeline outputs.

- `--results results/toy_experiment`  
  directory where summarised CSVs and per-run archives will go.

You can add extra options if desired:

- `--no-graph`  
  disable the Motivo (graph / Gaifman) pipeline to run only the hypergraph versions.


### 4.3 Expected outputs

After a successful run, you should see:

#### 4.3.1 Preprocessing summary

In `results/toy_experiment/`:

```text
toy_hg_preproc.csv
```

This CSV contains one row per **preprocessing stage**, with columns such as:

- date
- stage (build_hypergraph, split, gaifman_full, gaifman_low, dedup, …)
- variant (orig / dedup)
- input / output basenames
- number of threads
- walltime / usertime / systemtime
- log file name

This file is the main quick check that all preprocessing steps completed.

#### 4.3.2 Per-(K,T,S) pipeline CSVs

For each combination of:

- `K` (e.g. 3)  
- `T` (number of threads, e.g. 2)  
- `S` (samples, e.g. 10000)

you will get step-level CSV summaries in `results/toy_experiment/`. For the minimal command above (no dedup, graph enabled), you should see:

- `toy_hg_hyper_K3_T2_S10000.csv`  
  Hypergraph pipeline (HyperMotivo) step-level performance (either copied from `.perf` or `.timings.csv`).

- `toy_hg_gaifman_K3_T2_S10000.csv`  
  Graph pipeline (Motivo on Gaifman) step-level performance.

If you enable deduplication (`--deduplicate`) you will additionally see:

- `toy_hg_hyper_dedup_K3_T2_S10000.csv`  
  Hypergraph pipeline on the **deduplicated** version of the dataset.

If you also set `--early-stop`, there will be further files:

- `toy_hg_hyper_noes_K3_T2_S10000.csv`  
- `toy_hg_hyper_dedup_noes_K3_T2_S10000.csv`  

corresponding to hyper runs **without early-stop** (no subtype pruning).

#### 4.3.3 Per-run archives

For each (K, T, S), the script creates a directory:

```text
results/toy_experiment/toy_hg_K3_T2_S10000/
```

This directory contains:

- logs (`*.log`)
- detailed timings (`*.timings.csv`)
- performance CSVs (`*.perf`) when available
- counter tables
- sampler outputs, etc.

All artifacts for that parameter triple are grouped here for convenience.  
If `--delete` is given to `run_experiments.sh`, only the most relevant files (logs, timings, perf, info) are kept and everything else is deleted to save space.

---

## 5. Summary

For a quick sanity check during artifact evaluation:

1. **Build** the project (Section 2).  
2. Run the **toy pipeline** via `hyper_motivo_mac.sh` (Section 3).  
3. Run `run_experiments.sh` on `data/toy_hg.txt` (Section 4) and verify that:
   - `toy_hg_preproc.csv` exists under `results/toy_experiment/`
   - `toy_hg_hyper_K3_T2_S10000.csv` (and optionally `toy_hg_gaifman_…`) are present
   - the per-run directory `results/toy_experiment/toy_hg_K3_T2_S10000/` contains logs and timings

Once these are in place, you can swap `data/toy_hg.txt` with any larger dataset used in the paper by just changing the `--hg`, `--output`, and `--results` arguments in the `run_experiments.sh` call.
