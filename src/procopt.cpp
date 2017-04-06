#include <algorithm>
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>

#include "pipeline.hpp"
#include "util.hpp"

struct PipelineRun {
    int F, J, K, L, R;
    double ipc;
    double prediction_accuracy;
};

int main() {
    PipelineOptions options = {};

    int f, j, k, l, r;

    std::ofstream outfile ("procopt.out");
    std::ofstream full_data ("procopt.full.out");

    std::vector<std::string> traces = {"traces/hmmer_branch.100k.trace",
                                       "traces/gcc_branch.100k.trace",
                                       "traces/gobmk_branch.100k.trace",
                                       "traces/mcf_branch.100k.trace"};

    for (std::string& trace: traces) {
        std::vector<Instruction> instructions;
        parse_trace(trace, instructions);

        std::cout << "Optimizing " << trace << std::endl;

        outfile << "# Results for " << trace << std::endl;
        outfile << "====================================================" << std::endl;

        full_data << "# Results for " << trace << std::endl;

        std::vector<PipelineRun> results;
        results.reserve(160);

        for (f = 4; f <= 8; f += 4) {
            for (j = 1; j <= 2; j++) {
                for (k = 1; k <= 2; k++) {
                    for (l = 1; l <= 2; l++) {
                        for (r = 1; r <= 10; r++) {
                            // Setup options
                            options.F = f;
                            options.J = j;
                            options.K = k;
                            options.L = l;
                            options.R = r;

                            // Setup a Pipeline simulator
                            Pipeline p (instructions, options);
                            p.start();

                            // Save results of run
                            PipelineRun pr = {};
                            pr.F = f;
                            pr.J = j;
                            pr.K = k;
                            pr.L = l;
                            pr.R = r;
                            pr.ipc = p.proc_stats.avg_inst_retired;
                            pr.prediction_accuracy = p.proc_stats.prediction_accuracy;

                            results.push_back(pr);
                        }
                    }
                }
            }
        }

        // Sort pipeline runs by IPC
        std::sort(results.begin(), results.end(), [](const PipelineRun& pr1, const PipelineRun& pr2) {
            return pr1.ipc > pr2.ipc;
        });

        const double best_ipc = results[0].ipc;
        const float target_ratio = 0.95;
        double ratio;

        std::vector<PipelineRun> candidates;

        full_data << "F,J,K,L,R,IPC,Accuracy,Ratio" << std::endl;

        for (PipelineRun& pr: results) {
            double ratio = pr.ipc / best_ipc;

            // Consider runs with >95% of best IPC
            if (pr.ipc > 0.95*best_ipc)
                candidates.push_back(pr);

            full_data << pr.F << "," << pr.J << "," << pr.K << ",";
            full_data << pr.L << "," << pr.R << "," << pr.ipc << ",";
            full_data << pr.prediction_accuracy*100 << "," << (ratio*100) << std::endl;
        }

        outfile << std::endl << "* >95% of Best IPC (" << best_ipc << ")" << std::endl;

        // Output all candidates
        for (PipelineRun& pr: candidates) {
            outfile << "- F: " << pr.F << " J: " << pr.J << " K: " << pr.K;
            outfile << " L: " << pr.L << " R: " << pr.R << std::endl;
            outfile << "--- Prediction accuracy: " << pr.prediction_accuracy*100 << "%" << std::endl;
            outfile << "--- Best IPC: " << best_ipc << ", Found IPC: " << pr.ipc << " (" << (pr.ipc / best_ipc)*100 << "%)" << std::endl;
        }

        // Find the best pipeline configuration
        std::sort(candidates.begin(), candidates.end(), [](const PipelineRun& pr1, const PipelineRun& pr2) {
            return (pr1.J + pr1.K + pr1.L + pr1.R) < (pr2.J + pr2.K + pr2.L + pr2.R);
        });

        // Best configuration
        PipelineRun& pr = candidates[0];

        outfile << std::endl << "* Cheapest Configuration" << std::endl;
        outfile << "- F: " << pr.F << " J: " << pr.J << " K: " << pr.K;
        outfile << " L: " << pr.L << " R: " << pr.R << std::endl;
        outfile << "--- Prediction accuracy: " << pr.prediction_accuracy*100 << "%" << std::endl;
        outfile << "--- Best IPC: " << best_ipc << ", Found IPC: " << pr.ipc << " (" << (pr.ipc / best_ipc)*100 << "%)" << std::endl;

        outfile << "====================================================" << std::endl << std::endl;

        full_data << "====================================================" << std::endl;

        std::cout << "Trace " << trace << " completed." << std::endl;
    }

    outfile.close();
    full_data.close();

    return 0;
}
