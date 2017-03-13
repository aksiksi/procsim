#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>

struct inputargs_t {
    int R, F, J, K, L;
    std::string trace_file;
};

void parse_args(int argc, char **argv, inputargs_t& args);
void exit_on_error(const std::string& msg);

#endif
