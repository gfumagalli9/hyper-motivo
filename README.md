# HyperMotivo

Tools for **counting and sampling induced sub‑hypergraphs (hypergraphlets)** via color coding, splitting, and inclusion–exclusion. It extends the original **Motivo** framework for graphs to **hypergraphs**, adding a LOW/HIGH splitting pipeline, Gaifman construction, and a non‑adaptive sampler.

> If you are looking for graph (not hypergraph) motif counting, see the original Motivo repository.

---

## Papers & citation

This code implements the algorithm described in the draft paper *“Counting Graphlets in Hypergraphs: Breaking the Quadratic‑Time Barrier via Splitting and Color Coding.”* If you use this code, please cite:

```
Bressan, M., Clemente, S., Fumagalli, G. (2025).
Counting Graphlets in Hypergraphs: Breaking the Quadratic‑Time Barrier via Splitting and Color Coding.
Draft manuscript.
```

For the original Motivo algorithm on graphs, see:

```
Bressan, M., Leucci, S., Panconesi, A. (2019).
Motivo: fast motif counting via succinct color coding and adaptive sampling.
PVLDB 12(11):1651–1663.
```

---

## Features (what’s new vs. Motivo)

- **Hypergraph I/O**: ASCII ⇄ binary converter (`motivo-hypergraph`).
- **LOW/HIGH splitting**: `motivo-hgsplit` computes an α‑split (LOW = edges of size ≤ α; HIGH = edges of size > α) and materializes the **LOW∩HIGH vertex‑pair set** used during DP.
- **Gaifman construction** on LOW: `motivo-gaifman` builds the Gaifman/primal graph of the LOW part (with options for estimation/streaming).
- **Hypergraph DP builder**: `motivo-hyper-build` builds treelet tables (TTC) over HIGH with inclusion–exclusion (IE) support.
- **NWS/IE utilities**: `motivo-nws` computes the k=1 IE/NWS tables; `motivo-merge` merges `.cnt` into compressed `.dtz`/`.ie.dtz`.
- **Non‑adaptive sampler**: `motivo-hyper-sample` produces estimates and (optionally) raw samples for k‑hypergraphlets.

---

## Requirements

- **C++17** compiler (GCC ≥ 9 or Clang ≥ 9)
- **CMake ≥ 3.12**
- Libraries:
  - [sparsehash] (libsparsehash-dev)
  - [nauty] (libnauty2-dev)
  - [lz4] (liblz4-dev)
  - Optional: **tcmalloc** (libgoogle-perftools-dev)
- Python 3 (optional) to use helper scripts in `scripts/`.

On Debian/Ubuntu:

```bash
sudo apt-get install build-essential cmake git \
    libsparsehash-dev libnauty2-dev liblz4-dev \
    libgoogle-perftools-dev # optional
```

---

## Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j
# (optional) use tcmalloc
# cmake -DCMAKE_BUILD_TYPE=Release -DUSE_TCMALLOC=yes ..
```

Binaries are written to `build/bin/`.

To run tests:

```bash
ctest
```

Additional CMake options:

- `-DOPTIMIZE_MORE=YES` enables extra optimizations (e.g., `-march=native`).
- `-DENABLE_ASSERTS=YES` keeps runtime asserts in Release.
- `-DMOTIVO_OVERFLOW_SAFE=NO` disables big‑integer overflow checks (faster, less safe).

---

## Input formats

### Graphs (for comparison and for LOW’s Gaifman)

Same textual and binary formats as Motivo. See `bin/motivo-graph --help`.

### Hypergraphs

The **ASCII** input is one hyperedge per line: a list of non‑negative vertex IDs (any non‑digit is a separator). Duplicates within a hyperedge are removed; hyperedges are globally sorted by decreasing size at write time.

Convert ASCII ⇄ binary with `motivo-hypergraph`:

```bash
# ASCII → binary (produces: <base>.hmeta, .hef, .hvd, .vhef, .vhed)
bin/motivo-hypergraph --input my_hyper.txt --output data/hg

# binary → ASCII\ bin/motivo-hypergraph --dump --input data/hg --output data/hg.txt
```

Binary layout (read‑only):

- `<base>.hmeta` : `[num_vertices][num_edges]`
- `<base>.hef`   : offsets for edge→vertex adjacency (size = `num_edges+1`)
- `<base>.hvd`   : concatenated vertex IDs for all edges
- `<base>.vhef`  : offsets for vertex→edge incidence (size = `num_vertices+1`)
- `<base>.vhed`  : concatenated edge IDs for all vertices

---

## Quick start (hypergraphs)

> **The easy way** is to use the wrapper `scripts/hyper_motivo_mac.sh`, which orchestrates splitting, Gaifman(LOW), DP builds, NWS/IE merges, and sampling, and writes timings to CSV.

```bash
# Example: build k=5 TTCs and take 100k samples, 8 threads
BUILDPATH=build/bin \
./scripts/hyper_motivo_mac.sh --build --sample \
  -g data/hg              \  # basename of the full hypergraph
  -k 5                    \  # target k
  -o runs/hg_k5           \  # basename for outputs
  -t 8                    \  # threads
  -S 100000                  # number of samples (optional time budget)
```

Outputs (under `runs/hg_k5*`):

- `*.timings.csv` step‑by‑step wall times; `*.perf` extended CSV with per‑step details.
- TTC/IE tables: `<base>.<k>.{cnt,dtz,info,rts}` and `<base>.<k>.ie.{cnt,dtz}` for both LOW/HIGH and GLOBAL.
- Sampler CSV: `<base>.sample<k>.csv` with columns described below.

You can skip the split if you already have basenames for **HIGH** (hypergraph) and **LOW** (its Gaifman):

```bash
./scripts/hyper_motivo_mac.sh --build --sample \
  -H data/hg-High -L data/hg-LowG -k 5 -o runs/hg_k5 -t 8 -S 100000
```

---

## Manual pipeline (hard way)

Below is the equivalent sequence of commands if you prefer full control.

1) **Split LOW/HIGH** and compute the cross‑part **vertex pairs** used by DP:

```bash
# auto‑choose α (or pass --threshold <alpha>)
bin/motivo-hgsplit --input data/hg \
  --small data/hg-Low --large data/hg-High --threshold auto
```

2) **Gaifman on LOW** (produces a graph on the same basename):

```bash
bin/motivo-gaifman --input data/hg-Low --output data/hg-Low \
  --threads 8
```

3) **Build TTC at k=1** and initialize GLOBAL:

```bash
# HIGH (hypergraph)
bin/motivo-hyper-build --graph data/hg-High --size 1 --output runs/HighTTC
bin/motivo-merge --output runs/HighTTC.1 runs/HighTTC.1.cnt

# LOW Gaifman (graph)
bin/motivo-build --graph data/hg-Low --size 1 --output runs/LowTTC
bin/motivo-merge --output runs/LowTTC.1 runs/LowTTC.1.cnt

# NWS/IE (k=1) needed for k≥2 on HIGH
bin/motivo-nws --graph data/hg-High --size 1 \
  -i runs/HighTTC --output runs/HighTTC --threads 8
bin/motivo-merge -e --output runs/Global.1.ie runs/HighTTC.1.ie.cnt

# Initialize GLOBAL k=1 from HIGH k=1
cp runs/HighTTC.1.{dtz,info,rts} runs/Global.1.
```

4) **Iterate for k = 2..K**

```bash
# For each k≥2
k=2
# HIGH DP over hypergraph, using GLOBAL (lower) and IE tables
bin/motivo-hyper-build --graph data/hg-High --size $k \
  --output runs/HighTTC --lower runs/Global --ie runs/Global \
  --threads 8
bin/motivo-merge --output runs/HighTTC.$k runs/HighTTC.$k.cnt

# LOW DP over Gaifman (graph)
bin/motivo-build --graph data/hg-Low --size $k \
  --tables-basename runs/LowTTC --output runs/LowTTC --threads 8
bin/motivo-merge --output runs/LowTTC.$k runs/LowTTC.$k.cnt

# Fuse/merge into GLOBAL and IE
bin/motivo-merge --output runs/Global.$k \
  runs/HighTTC.$k.cnt runs/LowTTC.$k.cnt
bin/motivo-merge -e --output runs/Global.$k.ie runs/HighTTC.$k.ie.cnt
```

5) **Sampling** (non‑adaptive):

```bash
bin/motivo-hyper-sample \
  --gaifman data/hg-Low \
  --hypergraph data/hg-High \
  --hypergraph-full data/hg \
  --tables runs/Global \
  --nws-high runs/HighTTC \
  --size 5 \
  --num-samples 100000 \
  --threads 8 \
  --graphlets --estimate-occurrences --canonicize \
  --output runs/hg_k5
```

---

## Command synopsis

- `motivo-hypergraph` — convert hypergraph ASCII ⇄ binary.
- `motivo-hgsplit` — α‑split into **LOW/HIGH** hypergraphs and compute cross‑part pairs.
- `motivo-gaifman` — Gaifman/primal graph builder for LOW; supports `--threads`, `--max-edge-size`, `--estimate-upper`, etc.
- `motivo-hyper-build` — DP builder on HIGH over hypergraphs; inputs: `--graph`, `--size`, `--output`, `--lower` (GLOBAL TTC), `--ie` (IE TTC from HIGH), and `--threads`.
- `motivo-build` — DP builder on graphs (used for LOW’s Gaifman).
- `motivo-nws` — builds NWS/IE tables for k=1 on HIGH; required for k≥2.
- `motivo-merge` — merges `.cnt` files into compressed tables; add `-e` to merge IE (`*.ie.cnt → *.ie.dtz`).
- `motivo-hyper-sample` — non‑adaptive sampler on the combined tables; supports `--num-samples` or `--time-budget`.

Run each binary with `--help` for the complete option list.

---

## Sampler output

`<basename>.csv` columns:

- `motif` — canonical signature of the hypergraphlet (ASCII encoding).
- `est_occurrences` — estimated absolute number of induced occurrences.
- `est_frequency` — relative frequency over all induced k‑hypergraphlets.
- `samples` — how many samples produced that motif.
- `sampling_algo` — `N` (non‑adaptive) at present.
- `spanning_trees` — number of spanning trees of the Gaifman projection.
- `vertices` — one example occurrence (k vertex IDs).

Utilities in `scripts/` can decode/plot motif signatures.

---

## Performance notes

- Use `-t 0` (where supported) to auto‑select `std::thread::hardware_concurrency()`.
- For large K, consider `--store-on-0-colored-vertices-only` on the final build to reduce disk footprint.
- `motivo-gaifman` supports streaming/estimation modes for huge hyperedges.

---

## License

MIT License. See `LICENSE`.

---

## Acknowledgments

Motivo (graphs) by Marco Bressan, Stefano Leucci, and Alessandro Panconesi. This repository extends the framework to hypergraphs and adds specialized tools for LOW/HIGH splitting, IE/NWS tables, and hypergraph sampling.

