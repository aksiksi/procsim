#ifndef PREDICTOR_HPP
#define PREDICTOR_HPP

#include <vector>

/*
 * Implementation of n-entry k-bit Smith counter GSelect with GHR.
 * All Smith counter initialized to 01. GHR = 000 by default.
 */
class BranchPredictor {
public:
    BranchPredictor(int n);
    bool predict(int address);
    void update(int address, bool taken);
private:
    std::vector<std::vector<int>> prediction_table;
    int n; // Number of Smith counter entries
    int k; // Smith counter size, in bits

    int ghr;
    int ghr_size; // Size of GHR, in bits
};

#endif
