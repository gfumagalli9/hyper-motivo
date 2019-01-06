# Motivo

Motivo is a collection of tools for counting and sampling motifs in large graphs.
It is written in C++ and targets x86_64 processors although it should compile on other architectures as well.

##Setup

###Requirements

Motivo depends on the following libraries:

- [Google's sparsehash library](https://github.com/sparsehash/sparsehash),
- [OpenBLAS](http://www.openblas.net/) (or any other BLAS library),
- [LAPACKE](http://www.netlib.org/lapack/lapacke.html) if not already provided by your blas library,
- [Nauty](http://pallini.di.uniroma1.it/),
- [LZ4](https://github.com/lz4/lz4),
- Optional: libtcmalloc from [gperftools](https://github.com/gperftools/gperftools).

Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install lib{sparsehash,openblas,lapacke,nauty2}-dev
~~~~

And, if you want to use the tcmalloc allocator:
~~~~
# apt-get install libgoogle-perftools-dev
~~~~

A C++14 aware compiler is required along with support for [u]int{8,16,32,64} types.
Support for [mmap](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html) (POSIX.1-2001 and later) function is also currently required.

###Compiling

Install CMake (>= 3.5), checkout the source files and run:

~~~~
$ mkdir build
$ cd build
$ cmake ..
$ make
~~~~

If you want to use tcmalloc add the option -DUSE_TCMALLOC=yes to the cmake command line, i.e.:
~~~~
cmake -DUSE_TCMALLOC=yes
~~~~

If you prefer to build with Clang/LLVM (and your default compiler is different) use:

~~~
$ CC=clang CXX=clang++ cmake -D_CMAKE_TOOLCHAIN_PREFIX=llvm- ..
~~~

###Running the tests

~~~~
$ make test
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

###Installing

~~~
# make install
~~~

On Linux motivo is installed in /usr/local by default. If you wish to chose another directory you can pass the option -DCMAKE_INSTALL_PREFIX:PATH=/your/path to the cmake invocation, e.g.:

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

###Additional options

In addition to -DCMAKE_BUILD_TYPE=... you can pass the option -DOPTIMIZE_MORE=YES to cmake to enable additional optimization flags including -march=native. The resulting binaries might not work on other machines.

The option -DENABLE_ASSERTS=YES enables asserts even when the code is compiled in release mode (the default setting). These perform additional sanity checks during the computation but result in slower code.

The option -DMOTIVO_OVERFLOW_SAFE=NO disables overflow checks on arithmetic operations involving large numbers. This results in faster (but less safe) code. 

Example:

~~~
$ cmake -DCMAKE_BUILD_TYPE=Release -DOPTIMIZE_MORE=YES -DMOTIVO_OVERFLOW_SAFE=NO ..
~~~

##Usage

###Graph format

Motivo uses its own binary graph format. The tool motivo-graph allows to convert between a text representation of the graph to motivo's binary format, and vice-versa.
All graphs are simple, undirected, and loop-free. Vertices are consecutive integers starting from 0.

#### Textual graph format

A graph G with n vertices and m edges is encoded in a text file containing n+1 lines as follows:

 - The first line contains the integers n and m, separated by a space. Notice that m is the number edges of the *undirected* graph G (i.e., half the sum of the vertices' degrees).
 - For i>=0, The (i+1)th line encodes the neighbors of vertex i. It contains d+1 space-separated integers, where d is the degree of vertex i in G. The first integer is d and the remaining d integers are the neighbors of vertex i, in ascending order. 

#### Converting textual format to binary format

You can use
~~~
$ motivo-graph --input <text_graph> --output <basename>
~~~
to convert file <text_graph> in textual graph format to Motivo's binary format, which consists of two files: <basename>.gof and <basename>.ged

Example:
~~~
$ motivo-graph --input test-graph.txt --output test-graph
~~~

#### Converting binary format to textual format

You can also convert a graph in binary format back to its textual format:
~~~
$ motivo-graph --dump --input <basename> --output <text_graph>
~~~

Example:
~~~
$ motivo-graph --input test-graph --output test-graph-dump.txt
$ diff -bs test-graph.txt test-graph-dump.txt
Files test-graph.txt and test-graph-dump.txt are identical
~~~

#### Converting list-of-edges format to textual format

TODO

### Building tables

TODO

### Sampling

TODO

##Bug reports

Here: https://bitbucket.org/steven_/motivo/issues

##License

Yet to be chosen.