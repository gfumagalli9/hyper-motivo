# Motivo

Motivo is a tool suite for counting and sampling motifs in large graphs.
It is written in C++ and targets x86_64 processors.

##Requirements

Motivo depends on [Google's sparsehash library](https://github.com/sparsehash/sparsehash) 
and on the [GNU Scientific Library](https://www.gnu.org/software/gsl/).
Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install libsparsehash-dev libgsl-dev
~~~~

A C++11 aware compiler is required along with support for [u]int{8,16,32,64} types.

##Compiling

Install CMake (>= 3.6), checkout the source files and run.

~~~~
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .
~~~~

##Usage

Yet to come.

##Bug reports

Here: https://bitbucket.org/steven_/motivo/issues

##License

Yet to be chosen.