#include <vector>

#include "predictor.hpp"

BranchPredictor::BranchPredictor(int n, int k) : n(n), k(k) {
    // GHR setup
    int ghr_size = 3;
    int ghr = 0;

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

    if (counter >= (1 << k)/2)
        return true;
    else
        return false;
}

void BranchPredictor::update(int address, bool taken) {
    // Update Smith counter value based on branch result
    int hash = (address / 4) % n;
    std::vector<int>& counters = prediction_table[hash];

    if (taken) {
        if (counters[ghr] < (1 << k) - 1)
            counters[ghr] = counters[ghr] + 1;
    }

    else {
        if (counters[ghr] > 0)
            counters[ghr] = counters[ghr] - 1;
    }

    // Update GHR and table based on branch result
    int max_size = 1 << ghr_size;

    if (taken) {
        // Right shift 1 into GHR
        ghr = ((ghr << 1) + 1) & ((1 << max_size) - 1);
    } else {
        // Right shift 0 into GHR
        ghr = (ghr << 1) & ((1 << max_size) - 1);
    }
}
