# A Tomasulo Pipeline

This code was written for Project 2 of ECE6100 at Georgia Tech. It is written in C++11, which is supported by most modern compilers.

## Building

Navigate to root directory, and run `make`.

Tested with:

* LLVM 7.3.0 on OS X 10.11.3
* Ubuntu
* Fedora

## Running

After building, an executable called `procsim` will be in the root directory.

The program takes the following arguments:

* `-f`: number of instructions fetched per cycle
* `-j`: number of k0 function units (FUs)
* `-k`: number of k1 FUs
* `-l`: number of k2 FUs
* `-r`: number of result buses (RBs)
* `-i`: input trace file

Example: `procsim -f 4 -j 3 -k 2 -l 1 -r 2 -i gcc.100k.trace`

The output file will have the same name but with the extension `.out` and written to the same directory. In the example above, `gcc.100k.trace.out`.
