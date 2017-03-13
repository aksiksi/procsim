#include <iostream>

// TODO: fill this in
struct input_args_t {
    int R, F, J, K, L;
    std::string trace_file;
} args;

// TODO: modify args reading
void parse_args(int argc, char **argv, inputargs_t& args) {
    // Variables for getopt()
    extern char *optarg;
    extern int optind;

    // Args string for getopt()
    static const char* ALLOWED_ARGS = "C:B:S:V:K:i:";
    
    int c;
    u64 num = 0;  // Stores converted arg from char* to uint64_t
    u64 *arg; // Pointer to struct arg (DRY)

    // Set defaults for args
    args.C = DEFAULT_C;
    args.B = DEFAULT_B;
    args.S = DEFAULT_S;
    args.V = DEFAULT_V;
    args.K = DEFAULT_K;
    args.trace_file = nullptr;

    while ((c = getopt(argc, argv, ALLOWED_ARGS)) != -1) {
        if (c == 'C' || c == 'B' || c == 'S' || c == 'V' || c == 'K')
            num = strtol(optarg, NULL, 10);
        
        switch (c) {
            case 'C':
                arg = &(args.C);
                break;
            case 'B':
                arg = &(args.B);
                break;
            case 'S':
                arg = &(args.S);
                break;
            case 'V':
                arg = &(args.V);
                break;
            case 'K':
                arg = &(args.K);
                break;
            case 'i':
                // Create a pointer for persistence
                // Then open the file in read mode
                std::ifstream *ifs = new std::ifstream();
                ifs->open(optarg);
                
                if (!ifs->good())
                    exit_on_error("File not found.");

                args.trace_file = ifs;
        }
        
        if (c != 'i')
            *arg = static_cast<uint64_t>(num);
    }

    // Store number of blocks in cache
    args.N = args.C - args.B;

    // Error checking
    if (args.B > args.C)
        exit_on_error("B cannot be greater than C.");
}

int main(int argc, char** argv) {

}
