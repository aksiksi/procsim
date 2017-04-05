#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>

#include "pipeline.hpp"
#include "util.hpp"

int main(int argc, char** argv) {
    // Unbuffered output
    std::cout.setf(std::ios::unitbuf);

    // Parse command line args
    InputArgs inputargs = {};
    parse_args(argc, argv, inputargs);

    // Input file
    std::ifstream trace_file (inputargs.trace_file);

    // Output results file
    std::string output_file = inputargs.trace_file + ".out";

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

        // If branch line, extract branch address and taken flag
        if (!iss.eof()) {
            iss >> std::hex >> inst.branch_addr >> std::dec;
            iss >> inst.taken;
        }

        inst.idx = idx++;

        instructions.push_back(inst);
    }

    trace_file.close();

    std::cout << "* Input file: " << inputargs.trace_file << std::endl;
    std::cout << "*** " << instructions.size() << " instructions read from trace file" << std::endl;
    std::cout << "* Pipeline started; please wait for results" << std::endl;

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

    std::cout << "*** Pipeline completed successfully (cycles=" << proc_stats.cycle_count << ")" << std::endl;

    // Final stats
    output.precision(12);
    output << std::endl << "Processor stats:" << std::endl;
    output << "Total instructions: " << proc_stats.total_instructions << std::endl;
    output << "Avg. dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    output << "Max dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    output << "Avg. inst. issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    output << "Avg. inst. retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    output << "Total run time: " << proc_stats.cycle_count << std::endl << std::endl;

    std::cout << "* Results written to: " << output_file << std::endl;

    output.close();

    // Same stats to stdout
    std::cout.precision(12);
    std::cout << std::endl << "Processor stats:" << std::endl;
    std::cout << "Total branch instructions: " << proc_stats.total_branches << std::endl;
    std::cout << "Total correctly predicted branches: " << proc_stats.correct_branches << std::endl;
    std::cout << "Prediction accuracy: " << proc_stats.prediction_accuracy << std::endl;
    std::cout << "Avg. dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    std::cout << "Max dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    std::cout << "Avg. inst. issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    std::cout << "Avg. inst. retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    std::cout << "Total run time (cycles): " << proc_stats.cycle_count << std::endl << std::endl;

    return 0;
}
