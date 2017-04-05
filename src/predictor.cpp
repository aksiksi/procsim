#include <vector>

#include "predictor.hpp"

BranchPredictor::BranchPredictor(int n, int k) : n(n), k(k) {
    // GHR setup
    ghr_size = 3;
    ghr = 0;

    // Initialize prediction table (entry = 01 by default)
    for (int i = 0; i < n; i++) {
        std::vector<int> counters;
        for (int j = 0; j < (1 << ghr_size); j++)
            counters.push_back(1);

        prediction_table.push_back(counters);
    }
}

bool BranchPredictor::predict(int address) {
    // Compute hash of address using given formula
    int hash = (address / 4) % n;

    // Perform lookup in table to find required Smith counter
    std::vector<int>& counters = prediction_table[hash];
    int counter = counters[ghr];

    bool prediction = counter >= (1 << s)/2;

    return prediction;
}

void BranchPredictor::update(int address, bool taken) {
    // Update Smith counter value based on actual branch result
    int hash = (address / 4) % n;
    std::vector<int>& counters = prediction_table[hash];

    if (taken) {
        if (counters[ghr] < (1 << s) - 1)
            counters[ghr] = counters[ghr] + 1;
    }

    else {
        if (counters[ghr] > 0)
            counters[ghr] = counters[ghr] - 1;
    }

    // Update GHR and table based on branch result
    if (taken) {
        // Right shift 1 into GHR
        ghr = ((ghr << 1) + 1) & ((1 << ghr_size) - 1);
    } else {
        // Right shift 0 into GHR
        ghr = (ghr << 1) & ((1 << ghr_size) - 1);
    }
}
