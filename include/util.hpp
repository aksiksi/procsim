#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

#include "pipeline.hpp"

struct InputArgs {
    int R, F, J, K, L;
    std::string trace_file;
};

void parse_args(int argc, char **argv, InputArgs& args);
void exit_on_error(const std::string& msg);
void parse_trace(std::string file, std::vector<Instruction>& instructions);

#endif
