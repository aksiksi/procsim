#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

struct TraceLine {
    int addr;
    int fu_type;
    int dest_reg;
    int src1_reg;
    int src2_reg;
    int branch_addr;
    bool taken;
};

struct InputArgs {
    int R, F, J, K, L;
    std::string trace_file;
};

void parse_args(int argc, char **argv, InputArgs& args);
void exit_on_error(const std::string& msg);

#endif
