# Project 2

## Build

Run `make` in root directory. To build pipeline optimizer, run `make procopt`.

## Run

### Pipeline Simulator

Format: `./procsim -f F -r R -j J -k K -l L -i <trace_file>`

Example: `./procsim -f 4 -r 2 -j 3 -k 2 -l 1 -i gcc_branch.100k.trace`

Stats are printed to both `stdout` as well as the end of the output file.

The output file will have the same name but with the extension `.out` and written to the same directory. For the example above, the output file will be `gcc.100k.trace.out`.

### Pipeline Optimizer

Firs, place traces in `traces/`, then run `./procopt`. Optimal configurations are output to file `procopt.out`. Full data in CSV format for each trace is output to `procopt.full.out`.
