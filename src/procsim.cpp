#include <iostream>

#include "procsim.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
    inputargs_t inputargs = {};
    parse_args(argc, argv, inputargs);

    std::cout << inputargs.trace_file << " " << inputargs.F << std::endl;

    return 0;
}
