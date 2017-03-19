#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include "pipeline.hpp"
#include "util.hpp"

#define DEBUG

int main(int argc, char** argv) {
    #ifdef DEBUG
        // Unbuffered output
        std::cout.setf(std::ios::unitbuf);
    #endif

    // Parse command line args
    InputArgs inputargs = {};
    parse_args(argc, argv, inputargs);

    std::ifstream trace_file (inputargs.trace_file);

    // Stores data for all trace file lines
    std::vector<Instruction> instructions;
    Instruction inst;

    if (!trace_file.is_open())
        exit_on_error("Unable to open trace file specified (" + inputargs.trace_file + ")");

    std::string line;

    int idx = 0;

    while (getline(trace_file, line)) {
        std::istringstream iss (line);

        inst = {};
        iss >> std::hex >> inst.addr >> std::dec;
        iss >> inst.fu_type;
        iss >> inst.dest_reg;
        iss >> inst.src_reg[0];
        iss >> inst.src_reg[1];

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

    // Output results to file
    std::string output_file = inputargs.trace_file + ".out";
    std::ofstream output (output_file);

    // Pipeline settings
    output << "Processor settings:" << std::endl;
    output << "R: " << opt.R << std::endl;
    output << "k0: " << opt.J << std::endl;
    output << "k1: " << opt.K << std::endl;
    output << "k2: " << opt.L << std::endl;
    output << "F: " << opt.F << std::endl << std::endl;

    output << "INST  " << "FETCH  " << "DISP  " << "SCHED  " << "EXEC  " << "STATE  " << std::endl;

    // Cycle-by-cycle results
    for (InstStatus& is: p.status) {
        output << is.idx+1 << " ";
        output << is.fetch+1 << " ";
        output << is.disp+1 << " ";
        output << is.sched+1 << " ";
        output << is.exec+1 << " ";
        output << is.state+1 << std::endl;
    }

    Stats proc_stats = p.proc_stats;

    // Final stats
    output.precision(10);
    output << std::endl << "Processor stats:" << std::endl;
    output << "Total instructions: " << proc_stats.total_instructions << std::endl;
    output << "Avg. dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    output << "Max dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    output << "Avg. inst. issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    output << "Avg. inst. retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    output << "Total run time: " << proc_stats.cycle_count << std::endl << std::endl;

    output.close();

    // Same stats to stdout
    std::cout.precision(10);
    std::cout << std::endl << "Processor stats:" << std::endl;
    std::cout << "Total instructions: " << proc_stats.total_instructions << std::endl;
    std::cout << "Avg. dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    std::cout << "Max dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    std::cout << "Avg. inst. issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    std::cout << "Avg. inst. retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    std::cout << "Total run time: " << proc_stats.cycle_count << std::endl << std::endl;

    return 0;
}
