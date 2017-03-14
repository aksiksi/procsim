#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include "procsim.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
    // Parse command line args
    InputArgs inputargs = {};
    parse_args(argc, argv, inputargs);

    std::ifstream trace_file (inputargs.trace_file);

    // Stores data for all trace file lines
    std::vector<TraceLine> traces;
    TraceLine tl;

    if (!trace_file.is_open())
        exit_on_error("Unable to open trace file specified.");

    std::string line;

    while (getline(trace_file, line)) {
        std::istringstream iss (line);

        tl = {};
        iss >> std::hex >> tl.addr >> std::dec;
        iss >> tl.fu_type;
        iss >> tl.dest_reg;
        iss >> tl.src1_reg;
        iss >> tl.src2_reg;

        traces.push_back(tl);
    }

    trace_file.close();

    return 0;
}
