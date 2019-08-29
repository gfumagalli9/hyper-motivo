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

##Setup

###Requirements

Motivo depends on the following libraries:

- [Google's sparsehash library](https://github.com/sparsehash/sparsehash),
- [Nauty](http://pallini.di.uniroma1.it/),
- [LZ4](https://github.com/lz4/lz4),
- Optional: libtcmalloc from [gperftools](https://github.com/gperftools/gperftools).

Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install libsparsehash-dev libnauty2-dev
~~~~

And, if you want to use the tcmalloc allocator:
~~~~
# apt-get install libgoogle-perftools-dev
~~~~

A C++17 aware compiler is required along with support for [u]int{8,16,32,64,128} types.
Support for the [mmap](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html) (POSIX.1-2001 and later) function is also currently required.

###Compiling

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

###Running the tests

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

###Installing

~~~
# make install
~~~

On Linux, motivo is installed in /usr/local by default. If you wish to chose another directory you can pass the option -DCMAKE_INSTALL_PREFIX:PATH=/your/path to the cmake invocation, e.g.:

~~~
$ cmake -DCMAKE_INSTALL_PREFIX:PATH=~/motivo ..
~~~

###Building a Debian package

If you prefer to install a Debian package, you can build one by running:

~~~
$ make package
~~~

This will generate a package named "Motivo-<version>-Linux.deb", to install it run:

~~~
# dpkg -i Motivo-<version>-Linux.deb
# apt-get install -f
~~~

###Additional build< options

In addition to `-DCMAKE_BUILD_TYPE=...` you can pass the option `-DOPTIMIZE_MORE=YES` to cmake to enable additional optimization flags including `-march=native`. The resulting binaries might not work on other machines.

The option `-DENABLE_ASSERTS=YES` enables asserts even when the code is compiled in release mode (the default setting). These perform additional sanity checks during the computation but result in slower code.

The option `-DMOTIVO_OVERFLOW_SAFE=NO` disables overflow checks on arithmetic operations involving large numbers. This results in faster (but less safe) code. 

Example:

~~~
$ cmake -DCMAKE_BUILD_TYPE=Release -DOPTIMIZE_MORE=YES -DMOTIVO_OVERFLOW_SAFE=NO ..
~~~

##Usage

###Graph format

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


### Building the tables

#### Building the first table
~~~
$ bin/motivo-build -g test-graph --size 1 --colors 5 --output tables
[...]
Loaded graph with 60 vertices and 159 edges
Computing counts of treelets of size 1 for vertices 0--59 using 1 thread(s)
Building time: 2.8309e-05 s
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
Merge time: 0.000682108 s
~~~


#### Building the other tables

~~~
$ bin/motivo-build -g test-graph --size 2 --tables-basename tables --output tables --threads 0
[...]
Loaded graph with 60 vertices and 159 edges
Loading tables for smaller sizes
Computing counts of treelets of size 2 for vertices 0--59 using 4 thread(s)
Building time: 0.000835162 s
Output written to tables.2.cnt
~~~

~~~
$ bin/motivo-merge --output tables.2 tables.2.cnt
~~~

~~~
$ bin/motivo-build -g test-graph --size 3 --tables-basename tables --output tables --threads 0
$ bin/motivo-merge --output tables.3 tables.3.cnt
$
$ bin/motivo-build -g test-graph --size 4 --tables-basename tables --output tables --threads 0
$ bin/motivo-merge --output tables.4 tables.4.cnt
$
$ bin/motivo-build -g test-graph --size 5 --tables-basename tables --output tables --threads 0
$ bin/motivo-merge --output tables.5 tables.5.cnt
~~~

### Sampling

~~~

~~~

### Advanced options

TODO

##Bug reports

Here: https://bitbucket.org/steven_/motivo/issues

##License

Motivo is released under the MIT License. Please see the file `LICENSE` provided with the source code for details. 