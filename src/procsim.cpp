#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include "pipeline.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
    // Parse command line args
    InputArgs inputargs = {};
    parse_args(argc, argv, inputargs);

    std::ifstream trace_file (inputargs.trace_file);

    // Stores data for all trace file lines
    std::vector<Instruction> instructions;
    Instruction inst;

    if (!trace_file.is_open())
        exit_on_error("Unable to open trace file specified.");

    std::string line;

    int idx = 0;

    while (getline(trace_file, line)) {
        std::istringstream iss (line);

        inst = {};
        iss >> std::hex >> inst.addr >> std::dec;
        iss >> inst.fu_type;
        iss >> inst.dest_reg;
        iss >> inst.src1_reg;
        iss >> inst.src2_reg;

        // TODO: Parse branch lines also

        inst.idx = idx++;

        instructions.push_back(inst);
    }

    trace_file.close();

    // Setup pipeline options
    PipelineOptions opt = {
        .F = inputargs.F,
        .J = inputargs.J,
        .K = inputargs.K,
        .L = inputargs.L,
        .R = inputargs.R
    };

    // Create a new pipeline
    Pipeline p (instructions, opt);

    p.start();

    return 0;
}
