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

    // Output results file
    std::string output_file = inputargs.trace_file + ".out";

    // Parse trace lines and store data in a vector of Instruction
    std::vector<Instruction> instructions;
    parse_trace(inputargs.trace_file, instructions);

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
    output << "Processor Settings" << std::endl;
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
    output.precision(8);
    output << std::endl << "Processor stats:" << std::endl;
    output << "Total branch instructions: " << proc_stats.total_branches << std::endl;
    output << "Total correct predicted branch instructions: " << proc_stats.correct_branches << std::endl;
    output << "prediction accuracy: " << proc_stats.prediction_accuracy << std::endl;
    output << "Avg Dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    output << "Maximum Dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    output << "Avg inst Issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    output << "Avg inst retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    output << "Total run time (cycles): " << proc_stats.cycle_count << std::endl;

    std::cout << "* Results written to: " << output_file << std::endl;

    output.close();

    // Same stats to stdout
    std::cout.precision(8);
    std::cout << std::endl << "Processor stats:" << std::endl;
    std::cout << "Total branch instructions: " << proc_stats.total_branches << std::endl;
    std::cout << "Total correct predicted branch instructions: " << proc_stats.correct_branches << std::endl;
    std::cout << "prediction accuracy: " << proc_stats.prediction_accuracy << std::endl;
    std::cout << "Avg Dispatch queue size: " << proc_stats.avg_disp_size << std::endl;
    std::cout << "Maximum Dispatch queue size: " << proc_stats.max_disp_size << std::endl;
    std::cout << "Avg inst Issue per cycle: " << proc_stats.avg_inst_issue << std::endl;
    std::cout << "Avg inst retired per cycle: " << proc_stats.avg_inst_retired << std::endl;
    std::cout << "Total run time (cycles): " << proc_stats.cycle_count << std::endl;

    return 0;
}
