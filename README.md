# Motivo

Motivo is a tool suite for counting and sampling motifs in large graphs.
It is written in C++ and targets x86_64 processors.

##Requirements

Motivo depends on the following libraries:

- [Google's sparsehash library](https://github.com/sparsehash/sparsehash) 
- [C Minimal Perfect Hashing Library](http://cmph.sourceforge.net/)
- [GNU Scientific Library](https://www.gnu.org/software/gsl/).

Your Linux distribution might have premade packages, i.e., on Debian you can run:
~~~~
# apt-get install libsparsehash-dev libcmph-dev libgsl-dev
~~~~

A C++14 aware compiler is required along with support for [u]int{8,16,32,64} types.
Support for [mmap](http://pubs.opengroup.org/onlinepubs/9699919799/functions/mmap.html) (POSIX.1-2001 and later) function is also currently required.

##Compiling

Install CMake (>= 3.6), checkout the source files and run.

~~~~
$ mkdir build
$ cd build
$ cmake ..
$ cmake --build .
~~~~

###Running the tests

Simply run:

~~~~
./Motivo-tests
~~~~

You will get an output similar to the following:

~~~~
[doctest] doctest version is "1.1.3"
[doctest] run with "--help" for options
===============================================================================
[doctest] test cases:    2 |    2 passed |    0 failed |    0 skipped
[doctest] assertions:   26 |   26 passed |    0 failed |
~~~~

##Usage

Yet to come.

##Bug reports

Here: https://bitbucket.org/steven_/motivo/issues

##License

Yet to be chosen.