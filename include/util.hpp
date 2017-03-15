#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

struct InputArgs {
    int R, F, J, K, L;
    std::string trace_file;
};

void parse_args(int argc, char **argv, InputArgs& args);
void exit_on_error(const std::string& msg);

#endif
