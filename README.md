# Motivo

Motivo is a collection of tools for counting and sampling motifs in large graphs — and now **hypergraphs**.  
It is written in C++ and targets x86_64 processors, although it should compile on other architectures as well.

Motivo is described in [this paper](https://arxiv.org/abs/1906.01599). If you publish results based on Motivo, please acknowledge us by citing:
```
M. Bressan, S. Leucci, A. Panconesi.
Motivo: fast motif counting via succinct color coding and adaptive sampling.
PVLDB, 12(11):1651-1663, 2019.
DOI: https://doi.org/10.14778/3342263.3342640
```

---

## Setup

### Requirements

Motivo depends on the following libraries:

- [Google's sparsehash](https://github.com/sparsehash/sparsehash)
- [nauty](http://pallini.di.uniroma1.it/)
- [LZ4](https://github.com/lz4/lz4)
- Optional: libtcmalloc from [gperftools](https://github.com/gperftools/gperftools)

Your Linux distribution might have premade packages, i.e., on Debian you can run:
```bash
sudo apt-get install libsparsehash-dev libnauty2-dev liblz4-dev
# Optional:
sudo apt-get install libgoogle-perftools-dev
```

A C++17 aware compiler is required along with support for [u]int{8,16,32,64,128} types.  
Support for `mmap` (POSIX.1-2001 and later) is also currently required.

> **Python helpers (optional).** Some utilities for visualizing motifs/hypergraphlets use Python:
> - `numpy`, `matplotlib`
> - For graphlets: `networkx`
> - For hypergraphlets: **`hypernetx`** (and its dependency `fastjsonschema`)
>
> Install with:
> ```bash
> python3 -m pip install numpy matplotlib networkx hypernetx fastjsonschema
> ```

### Missing libraries

If any of the required C/C++ libraries are not available in your distribution, you can install them locally. Here we use `$HOME/local` as a prefix so nothing touches system folders.

```bash
mkdir -p "$HOME/local"
export CPATH="$CPATH:$HOME/local/include"
export LIBRARY_PATH="$LIBRARY_PATH:$HOME/local/lib"
```

**LZ4**
```bash
git clone https://github.com/lz4/lz4.git
cd lz4 && make && make install prefix="$HOME/local" && cd ..
```

**sparsehash**
```bash
git clone https://github.com/sparsehash/sparsehash
cd sparsehash
./configure --prefix="$HOME/local"
make && make install
cd ..
```

**nauty** (example version; check for newer)
```bash
wget https://pallini.di.uniroma1.it/nauty27r3.tar.gz
tar xvzf nauty27r3.tar.gz
cd nauty27r3
./configure --enable-tls --prefix="$HOME/local"
make && make install
cd ..
```

### Compiling

Use CMake (>= 3.12):

```bash
mkdir build && cd build
cmake ..
make -j
```

The compiled binaries end up in `build/bin/`.

Enable tcmalloc:
```bash
cmake -DUSE_TCMALLOC=YES ..
```

Build with clang/LLVM:
```bash
CC=clang CXX=clang++ cmake -D_CMAKE_TOOLCHAIN_PREFIX=llvm- ..
```

### Running the tests

```bash
ctest
```

With a memory checker (e.g., valgrind):
```bash
ctest -T memcheck
```

### Installing

```bash
sudo make install
```
By default, Motivo installs into `/usr/local`. Override with:
```bash
cmake -DCMAKE_INSTALL_PREFIX:PATH="$HOME/motivo" ..
```

### Debian package

```bash
make package
# produces Motivo-<version>-Linux.deb
sudo dpkg -i Motivo-<version>-Linux.deb
sudo apt-get -f install
```

### Additional build options

- `-DCMAKE_BUILD_TYPE=Release` (default recommended)
- `-DOPTIMIZE_MORE=YES` enables extra flags including `-march=native` (may reduce portability)
- `-DENABLE_ASSERTS=YES` keeps asserts in Release
- `-DMOTIVO_OVERFLOW_SAFE=NO` disables overflow checks (faster, less safe)

Example:
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DOPTIMIZE_MORE=YES -DMOTIVO_OVERFLOW_SAFE=NO ..
```

---

## Input formats

### Graph format

Motivo uses its own binary graph format. The tool `motivo-graph` converts to/from textual representations.

All graphs are simple, undirected, and loop-free. Vertices are consecutive integers starting at 0.

#### Textual formats

All textual formats begin with line `n m` (number of vertices and **undirected** edges). Edges are listed twice (once per endpoint).

**List of edges (`-f LOE`)**
```
4 5
0 1
0 2
1 0
1 2
1 3
2 0
2 1
2 3
3 1
3 2
```

**One node per line (`-f NODE`)**
```
4 5
0 1 2
1 0 2 3
2 0 1 3
3 1 2
```

**One node per line, explicit degree (`-f NODE_DEGREE`)**
```
4 5
2 1 2
3 0 2 3
3 0 1 3
2 1 2
```

#### Converting textual <-> binary

```bash
# text -> binary
bin/motivo-graph --format NODE_DEGREE --input diamond.txt --output test-graph

# binary -> text
bin/motivo-graph --dump --format NODE_DEGREE --input test-graph --output test-graph-dump.txt
```

### Hypergraph format

Motivo’s hypergraph pipeline consumes a compact **binary** format (read by `Hypergraph`), composed of several companion files sharing a basename `<base>`:

- `<base>.hmeta` : two 32‑bit integers: `[vertex_t num_verts][edge_t num_edges]`
- `<base>.hef`   : `(num_edges + 1)` 32‑bit offsets into `.hvd` (hyperedge → vertices index)
- `<base>.hvd`   : concatenated `vertex_t` lists for each hyperedge (the vertices within each hyperedge **must be sorted** and contiguous in this file)
- `<base>.vhef`  : optional, `(num_verts + 1)` 32‑bit offsets into `.vhed` (vertex → incident hyperedges index)
- `<base>.vhed`  : optional, concatenated `edge_t` lists for each vertex (incident hyperedges, **sorted**)
- `<base>.dmap`  : optional, `num_verts` entries of `vertex_t` providing a dense→original vertex-id map

> Only `.hmeta`, `.hef`, `.hvd` are strictly required. If present, `.vhef/.vhed` accelerate membership/degree queries; `.dmap` allows round-tripping original ids.

**Notes & expectations**
- Types: `vertex_t = uint32_t`, `edge_t = uint32_t`.
- Offsets arrays (`.hef`, `.vhef`) have length `count+1`; the last element is the total number of items in the corresponding data file.
- All arrays are little‑endian on x86_64.
- The loader is read-only and mmaps these files.

---

## Basic usage

From `build/`, Motivo can be launched (1) the easy way via the wrapper `../scripts/motivo.sh`, or (2) directly via `bin/motivo-build` and `bin/motivo-sample`.

### The easy way (graphs)

Compute 5‑motif counts, take 100k samples, write `output.csv`:
```bash
../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output -s 100000
```

CSV columns:
- `motif`: ASCII signature of the motif (see `scripts/motivo_utils.py`)
- `est_occurrences`: estimated number of induced occurrences
- `est_frequency`: estimated relative frequency
- `samples`: how many copies appeared in the sample
- `sampling_algo`: `N` for naive, `A` for adaptive
- `spanning_trees`: number of spanning trees of the motif
- `vertices`: vertices of an occurrence

Reuse built tables to sample again:
```bash
../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --sample -s 100000
```
Build without sampling:
```bash
../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --build
```
Adaptive graphlet sampling:
```bash
../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --sample -s 100000 -a
```

### The hard way (graphs)

Build & merge tables for sizes 1..k, then sample with `bin/motivo-sample`. See the original README sections for full command examples.

---

## Hypergraph workflow

Motivo includes a hypergraph pipeline to **sample hypergraphlets** (small induced patterns in a hypergraph) and report them to CSV.

### CSV output (hypergraphs)

A typical header looks like:
```
hyper_motif, est_occurrences, est_frequency, samples, sampling_algo, spanning_trees, vertices
```

- `hyper_motif`: **hex**-encoded signature of the bipartite incidence pattern restricted to the sampled vertex set (see below)
- `est_occurrences`: estimated absolute number of induced occurrences
- `est_frequency`: estimated relative frequency
- `samples`: how many copies of this hypergraphlet were observed
- `sampling_algo`: `H` denotes the hypergraph sampler
- `spanning_trees`: number of spanning trees of the underlying treelet used by the sampler (for auditing; not the hypergraphlet)
- `vertices`: vertex ids of one representative occurrence

### Hypergraph signature format (what is `hyper_motif`?)

Each sampled hypergraphlet is serialized from its **bipartite incidence matrix** `M` with shape `k × b`:
- `k` = number of selected vertices
- `b` = number of distinct hyperedges that touch at least **two** of the selected vertices (weakly-induced incidence)

The signature is a byte-string encoded as hex:
```
[k:2 bytes][b:2 bytes][k*b bits row-major, MSB-first]
```
- The bit payload is `1` iff vertex `i` is contained in hyperedge/column `j` of the restricted pattern.
- Columns (hyperedges) with fewer than 2 selected vertices are **omitted**.
- Internally we optionally canonicalize the bipartite structure within its color classes (rows vs columns).

### Visualizing hypergraphlets

We provide a helper script `scripts/hyper_motivo_utils.py` to decode the CSV and generate one figure per hypergraphlet using **HyperNetX**.

Install deps:
```bash
python3 -m pip install numpy matplotlib hypernetx fastjsonschema
```

Usage:
```bash
# From the repository root (or adjust the path)
python3 scripts/hyper_motivo_utils.py <counts.csv> --outdir hyperplots --fmt pdf --limit 100
```

- The script reads the `hyper_motif` column, decodes its bipartite incidence, and draws the hypergraphlet to `<outdir>/<index>_<shortname>.<fmt>`.
- If the CSV includes the optional `vertices` column, those ids are used as labels.
- Each image also includes a small caption box with: `k`, `m` (number of kept hyperedges), and `samples` for that row.

**Troubleshooting**
- If drawings look too crowded, reduce node size or switch to PNG: `--fmt png`.

---

## Converting and plotting (graphs)

Motivo represents each (graph) motif as an ASCII string (e.g., `ADM` is the 5-star). This is a compact encoding of the **upper-triangular** adjacency matrix grouped into base‑16 nybbles mapped to `A..P` (see `scripts/motivo_utils.py`).

Example:
```python
>>> import motivo_utils as mu
>>> mu.signature_to_matrix("ADM")
array([[0,0,0,0,1],
       [0,0,0,0,1],
       [0,0,0,0,1],
       [0,0,0,0,1],
       [1,1,1,1,0]])
```

You can also plot all motifs from a CSV:
```bash
../scripts/motivo_utils.py plotmotif output.csv pdf
```

---

## Advanced options

TBD.

---

## Bug reports

https://gitlab.com/steven3k/motivo/-/issues

---

## License

Motivo is released under the MIT License. See `LICENSE` for details.
