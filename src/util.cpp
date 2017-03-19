#include <iostream>
#include <unistd.h>

#include "../include/util.hpp"

void print_usage() {
    std::cout << "Usage: ./procsim –r R –f F –j J –k K –l L -i <trace_file>" << std::endl;
    exit(EXIT_FAILURE);
}

void exit_on_error(const std::string& msg) {
    std::cout << "Error: " << msg << std::endl;
    exit(EXIT_FAILURE);
}

void parse_args(int argc, char **argv, InputArgs& args) {
    if (argc < 12)
        print_usage();

    // Variables for getopt()
    extern char *optarg;
    extern int optind;

    // Args string for getopt()
    static const char* ALLOWED_ARGS = "r:f:j:k:l:i:";

    int c;
    int num = 0;

    // Extract other parameters
    while ((c = getopt(argc, argv, ALLOWED_ARGS)) != -1) {
        // Stores converted arg from char* to int
        num = static_cast<int>(strtol(optarg, NULL, 10));

        switch (c) {
            case 'r':
                args.R = num;
                break;
            case 'f':
                args.F = num;
                break;
            case 'j':
                args.J = num;
                break;
            case 'k':
                args.K = num;
                break;
            case 'l':
                args.L = num;
                break;
            case 'i':
                args.trace_file = optarg;
                break;
            case '?':
            default:
                print_usage();
        }
    }

    if (args.trace_file.empty())
        args.trace_file = argv[argc-1];
}
