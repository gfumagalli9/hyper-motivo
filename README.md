# Motivo

Motivo is a collection of tools for counting and sampling motifs in large graphs.
It is written in C++ and targets x86_64 processors although it should compile on other architectures as well.

Motivo is described in [this paper](https://arxiv.org/abs/1906.01599). If you publish results based on Motivo, please acknowledge us by citing:
~~~
M. Bressan, S. Leucci, A. Panconesi.
Motivo: fast motif counting via succinct color coding and adaptive sampling.
PVLDB, 12(11):1651-1663, 2019.
DOI: https://doi.org/10.14778/3342263.3342640
~~~

## Setup

### Requirements

Motivo depends on the following libraries:

- [Google's sparsehash library](https://github.com/sparsehash/sparsehash),
- [Nauty](http://pallini.di.uniroma1.it/),
- [LZ4](https://github.com/lz4/lz4),
- Optional: libtcmalloc from [gperftools](https://github.com/gperftools/gperftools).

Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install libsparsehash-dev libnauty2-dev liblz4-dev
~~~~

And, if you want to use the tcmalloc allocator:
~~~~
# apt-get install libgoogle-perftools-dev
~~~~

A C++17 aware compiler is required along with support for [u]int{8,16,32,64,128} types.
Support for the [mmap](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html) (POSIX.1-2001 and later) function is also currently required.

### Missing libraries

If any of the required libraries are not available in your distribution, you can manually install them
Either follow the instructions in the corresponding sources to install them for the whole system or see below.

If you cannot or do not want to copy files into system-wide directories, you can  create a local prefix to install the missing libraries and headers
In this example I'm going to use `$HOME/local`

~~~~
mkdir $HOME/local

# This will ensure that Motivo's cmake knows where to look for libraries
export CPATH=$CPATH:$HOME/local/include
export LIBRARY_PATH=$LIBRARY_PATH:$HOME/local/lib
~~~~

To install lz4:
~~~~
git clone https://github.com/lz4/lz4.git
cd lz4
make
make install prefix=$HOME/local
cd ..
~~~~

To install Google's sparsehash:
~~~~
git clone https://github.com/sparsehash/sparsehash
cd sparsehash
./configure --prefix=$HOME/local
make
make install
cd ..
~~~~

To install Nauty (at the time of writing the latest version of nauty is 2.7r3, you might want to check for a more up to date version):
~~~~
wget https://pallini.di.uniroma1.it/nauty27r3.tar.gz
tar xvzf nauty27r3.tar.gz
cd nauty27r3
./configure --enable-tls --prefix=$HOME/local
make
make install
cd ..
~~~~

### Compiling

Install CMake (>= 3.12), checkout the source files and run:

~~~~
$ mkdir build
$ cd build
$ cmake ..
$ make
~~~~

The compiled files will be in the `bin` subdirectory.

If you want to use tcmalloc add the option `-DUSE_TCMALLOC=yes` to the cmake command line, i.e.:
~~~~
cmake -DUSE_TCMALLOC=yes
~~~~

If you prefer to build with Clang/LLVM (and your default compiler is different) use:

~~~
$ CC=clang CXX=clang++ cmake -D_CMAKE_TOOLCHAIN_PREFIX=llvm- ..
~~~

### Running the tests

~~~~
$ ctest
~~~~

Hopefully you will get an output similar to the following:

~~~~
Running tests...
Test project /home/steven/Projects/motivo/build
    Start 1: build-graph
1/2 Test #1: build-graph ......................   Passed    0.00 sec
    Start 2: motivo-tests
2/2 Test #2: motivo-tests .....................   Passed   32.24 sec

100% tests passed, 0 tests failed out of 2

Total Test time (real) =  32.25 sec
~~~~

If you want to run the tests with a memory checker (e.g., [valgrind](http://valgrind.org/)) use:

~~~~
$ ctest -T memcheck
~~~~

### Installing

~~~
# make install
~~~

You can find the Motivo binaries in the build/bin subdirectory. These can either be used directly or, if you wish, you can install them in your system.

On Linux, motivo is installed in /usr/local by default. If you wish to chose another directory you can pass the option -DCMAKE_INSTALL_PREFIX:PATH=/your/path to the cmake invocation, e.g.:

~~~
$ cmake -DCMAKE_INSTALL_PREFIX:PATH=~/motivo ..
~~~

### Building a Debian package

If you prefer to install a Debian package, you can build one by running:

~~~
$ make package
~~~

This will generate a package named "Motivo-<version>-Linux.deb", to install it run:

~~~
# dpkg -i Motivo-<version>-Linux.deb
# apt-get install -f
~~~

### Additional build options

In addition to `-DCMAKE_BUILD_TYPE=...` you can pass the option `-DOPTIMIZE_MORE=YES` to cmake to enable additional optimization flags including `-march=native`. The resulting binaries might not work on other machines.

The option `-DENABLE_ASSERTS=YES` enables asserts even when the code is compiled in release mode (the default setting). These perform additional sanity checks during the computation but result in slower code.

The option `-DMOTIVO_OVERFLOW_SAFE=NO` disables overflow checks on arithmetic operations involving large numbers. This results in faster (but less safe) code. 

Example:

~~~
$ cmake -DCMAKE_BUILD_TYPE=Release -DOPTIMIZE_MORE=YES -DMOTIVO_OVERFLOW_SAFE=NO ..
~~~

## Input format

### Graph format

Motivo uses its own binary graph format. The tool motivo-graph allows to convert between a text representation of the graph to motivo's binary format, and vice-versa.
All graphs are simple, undirected, and loop-free. Vertices are consecutive integers starting from 0.

#### Textual graph formats

All textual graph formats begin with a single line containing two integers `n` and `m`, separated by a space, representing
the number of vertices and of edges of the encoded graph `G`, respectively.
Notice that `m` is the number edges of the *undirected* graph G (i.e., half the sum of the vertices' degrees).
Nevertheless, all formats specify each edge `(u,v)` of `G` twice, once from vertex `u` and once from vertex `v`.

#### Converting the textual formats to binary format

You can use
~~~
$ bin/motivo-graph --format <format> --input <text_graph> --output <basename> 
~~~
to convert file <text_graph> in a textual graph format to Motivo's binary format, which consists of two files: <basename>.gof and <basename>.ged
See below for a list of supported formats and for the corresponding `-f` option.

Example:
~~~
$ bin/motivo-graph --format NODE_DEGREE --input diamond.txt --output test-graph
~~~

##### List of edges (-f LOE)

Each of subsequent line contains two integers `u` and `v`, separated by a space ad represents edge `(u,v)`.
The following example encodes a diamond graph:
~~~
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
~~~

##### One node per line (-f NODE)

Each subsequent line contains a list of integers `u v1 v2 v3 ...`
and represents node `u` in the graph along with all its incident edges `(u, v1), (u, v2), (u, v3), ...`.
The following example encodes a diamond graph:
~~~
4 5
0 1 2
1 0 2 3
2 0 1 3
3 1 2
~~~

##### One node per line, in order, with explicit degree (-f NODE_DEGREE)
The $i$-th subsequent line contains a list of integers `d v1 v2 v3 ...`
and represents the node `i-1` in the graph along with all its incident edges `(i-1, v1), (i-1, v2), (i-1, v3), ...`.
The following example encodes a diamond graph:
~~~
4 5
2 1 2
3 0 2 3
3 0 1 3
2 1 2
~~~


#### Converting the binary format back to textual format

You can also convert a graph in binary format back to its textual format:
~~~
$ bin/motivo-graph --dump --format <format> --input <basename> --output <text_graph>
~~~

Example:
~~~
$ bin/motivo-graph --format NODE_DEGREE --input ../graphs/test-graph.txt --output test-graph
$ bin/motivo-graph --format NODE_DEGREE --input test-graph --output test-graph-dump.txt
$ diff -bs ../graphs/test-graph.txt test-graph-dump.txt
Files ../graphs/test-graph.txt and test-graph-dump.txt are identical
~~~

## Basic usage

From the `build/` folder, Motivo can be launched in two ways: (1) the easy way, via the wrapper `../scripts/motivo.sh` (2) the hard way, via `bin/motivo-build` and `bin/motivo-sample`.

### The easy way

The following example will compute 5-motif counts, using 100000 samples, storing the result in `output.csv`.

~~~~
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output -s 100000
[Mon 01 Jan 1900 00:00:00 AM CEST] Done
size            build           merge           sample
1               .01             .04
2               .68             .07
3               .61             .10
4               .45             .12
5               .19             .02             .51
[Mon 01 Jan 1900 00:00:04 AM CEST] Done
Samples are in output.csv:
motif, est_occurrences, est_frequency, samples, sampling_algo, spanning_trees, vertices
AHE, 8.4178e+11, 4.5161e-01, 30040, N, 5, 19544 26346 16176 9899 3702
BFI, 4.0509e+11, 2.1733e-01, 14456, N, 5, 13422 7817 9903 22668 5188
ADM, 3.2281e+11, 1.7319e-01, 11520, N, 5, 11799 26937 3720 3626 3693
BFM, 8.6186e+10, 4.6238e-02, 9227, N, 15, 2038 11377 15212 22166 6125
AHM, 7.6537e+10, 4.1062e-02, 8194, N, 15, 20794 3714 7898 9805 2331
~~~~

The output can be read as follows:

- `motif`: the string signature of the motif (see `motivo_utils.py` for how to convert this signature, or how to plot the motif as a graph)
- `est_occurrences`: the estimated absolute number of induced occurrences of the motif in the graph
- `est_frequency`: the estimated relative frequency of induced occurrences of the motif in the graph
- `samples`: how many copies of this motif did appear in the sample
- `sampling_algo`: `N` for naive sampling, `A` for adaptive sampling
- `spanning_trees`: the number of spanning trees in the motif
- `vertices`: vertices of an occurrence of the motif in the graph

After the first run, you can use the tables built by Motivo to sample again at your will:

~~~
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --sample -s 100000
~~~

On the other hand, you can build the tables without sampling:

~~~
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --build
~~~

In fact, the first example is perfectly equivalent to:

~~~
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --build
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --sample -s 100000
~~~

All the examples above use the naive sampling. To use AGS (adaptive graphlet sampling):

~~~
$ ../scripts/motivo.sh -g /path/to/my/graph -k 5 -o output --sample -s 100000 -a
~~~

### The hard way

If you need more control over Motivo, you can build the tables yourself and then proceed to sampling.

#### Building the first table
~~~
$ bin/motivo-build -g test-graph --size 1 --colors 4 --output tables
[...]
Loaded graph with 60 vertices and 159 edges
Computing counts of treelets of size 1 for vertices 0--59 using 1 thread(s)
Building time: 9.2341e-05 s
Output written to tables.1.cnt
~~~

~~~
$ bin/motivo-merge --output tables.1 tables.1.cnt
[...]
Compress threshold is: 0
Loaded offsets for file tables.1.cnt vertices
Writing output
Compressed size: 3014 Original size: 2640 Ratio: 1.14167
Building root sampler alias table... done
Processed 60 vertices (wrote 60 counts)
Total number of treelet occurrences: 60 (6 bits)
Maximum number of occurrences rooted in a single vertex: 1 (1 bits)
Maximum number of occurrences of a single rooted treelet: 1 (1 bits)
Output written to files: tables.1.dtz, and tables.1.rts
Merge time: 0.00128869 s
~~~


#### Building the other tables

~~~
$ bin/motivo-build -g test-graph --size 2 --tables-basename tables --output tables --threads 0
[...]
Loaded graph with 60 vertices and 159 edges
Loading tables for smaller sizes
Computing counts of treelets of size 2 for vertices 0--59 using 4 thread(s)
Building time: 0.00138115 s
Output written to tables.2.cnt
~~~

~~~
$ bin/motivo-merge --output tables.2 tables.2.cnt
[...]
~~~

~~~
$ bin/motivo-build -g test-graph --size 3 --tables-basename tables --output tables --threads 0
$ bin/motivo-merge --output tables.3 tables.3.cnt
$
$ bin/motivo-build -g test-graph --size 4 --tables-basename tables --output tables --threads 0 --store-on-0-colored-vertices-only
$ bin/motivo-merge --output tables.4 tables.4.cnt
~~~

#### Sampling

~~~
$ bin/motivo-sample -g test-graph -i tables -s 4 -n 100000 --graphlets --estimate-occurrences --canonicize -o test --threads 0
[...]
Loaded graph with 60 vertices and 159 edges
Loading tables and root sampler
Using seed 6EA0024E9E1EB153
Sampling using 4 thread(s)
Using naive sampler
Naive sampler: taken 100000 samples in 0.800924 s
Sampling time: 0.861864 s
$ cat test.csv 
motif, est_occurrences, est_frequency, samples, sampling_algo, spanning_trees, vertices
AHE, 8.4178e+11, 4.5161e-01, 30040, N, 5, 19544 26346 16176 9899 3702
BFI, 4.0509e+11, 2.1733e-01, 14456, N, 5, 13422 7817 9903 22668 5188
ADM, 3.2281e+11, 1.7319e-01, 11520, N, 5, 11799 26937 3720 3626 3693
~~~

### Converting and plotting motifs

In the output, Motivo represents each motif as an ASCII string.
For instance, `ADM` is the star on 5 nodes.
This ASCII signature is, in fact, just a compact serialization of the adjacency matrix of the motif.
To convert the signature to other formats (adjacency list, adjacency matrix, edge list), use `scripts/motivo_utils.py`.

~~~
>>> import motivo_utils as mu
>>> mu.signature_to_matrix("ADM")
array([[0, 0, 0, 0, 1],
       [0, 0, 0, 0, 1],
       [0, 0, 0, 0, 1],
       [0, 0, 0, 0, 1],
       [1, 1, 1, 1, 0]])
~~~

~~~
>>> g = mu.Graphlet("ADM")
>>> g.n()
5
>>> g.m()
4
>>> g.edge_list()
array([[0, 4],
       [1, 4],
       [2, 4],
       [3, 4],
       [4, 0],
       [4, 1],
       [4, 2],
       [4, 3]])
~~~

You can also graphically plot each motif in the csv table, as in this example:

~~~
$ ../scripts/motivo_utils.py plotmotif output.csv
plotting from output.csv in pdf format
$ ls *.pdf
ADM.pdf AHM.pdf APM.pdf BFM.pdf BPM.pdf CPM.pdf EPM.pdf IFM.pdf JFM.pdf MNM.pdf PPM.pdf
AHE.pdf API.pdf BFI.pdf BPI.pdf COM.pdf DPM.pdf HPM.pdf IHM.pdf MIM.pdf MPM.pdf
~~~



### Advanced options

TODO

## Bug reports

Here: https://gitlab.com/steven3k/motivo/-/issues

## License

Motivo is released under the MIT License. Please see the file `LICENSE` provided with the source code for details. 
