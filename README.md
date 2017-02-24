# Motivo

Motivo is a collection of tools for counting and sampling motifs in large graphs.
It is written in C++ and targets x86_64 processors although it should compile on other architectures as well.

##Setup

###Requirements

Motivo depends on the following libraries:

- [Google's sparsehash library](https://github.com/sparsehash/sparsehash),
- [OpenBLAS](http://www.openblas.net/) (or any other BLAS library),
- [LAPACKE](http://www.netlib.org/lapack/lapacke.html),
- [Nauty](http://pallini.di.uniroma1.it/),
- [Boost.Program_options](http://www.boost.org/doc/libs/release/libs/program_options/)
- [Boost.Multiprecision](http://www.boost.org/doc/libs/releaselibs/multiprecision/)

Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install lib{sparsehash,openblas,lapacke,nauty2,boost-program-options,boost}-dev
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

You can pass the option -DOPTIMIZE_MORE=YES to cmake to enable additional optimization flags including -march=native. The resulting binaries might not work on other machines.

The option -DENABLE_ASSERTS=YES enables asserts even when the code is compiled in release mode (the default setting). These perform additional sanity checks during the computation but result in slower code.

The option -DMOTIVO_OVERFLOW_SAFE=YES enables overflow checks on arithmetic operations involving large numbers. This results in slower (but safer) code.

Example:

~~~
$ cmake -DOPTIMIZE_MORE=YES -DMOTIVO_OVERFLOW_SAFE=YES ..
~~~


##Usage

Yet to come.

##Bug reports

Here: https://bitbucket.org/steven_/motivo/issues

##License

Yet to be chosen.

