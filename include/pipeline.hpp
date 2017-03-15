#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <deque>
#include <vector>

struct PipelineOptions {
    int F, J, K, L, R;
};

// Single instruction as parsed from trace file
struct Instruction {
    int addr;
    int fu_type;
    int dest_reg;
    int src1_reg;
    int src2_reg;
    int branch_addr;
    bool taken;
};

// "Reservation Station"
// An entry in the scheduling queue
struct RS {
    int fu_type;
    int dest_reg;
    int dest_tag;
    bool src1_ready;
    int src1_tag;
    int src1_value;
    bool src2_ready;
    int src2_tag;
    int src2_value;
};

// Register as stored in register file
struct Register {
    int num, tag, value;
};

class Pipeline {
public:
    void start(std::vector<Instruction>& instructions);
    void flush();

    Pipeline(PipelineOptions& opt);

private:
    uint64_t clock;
    PipelineOptions options;

    std::deque<RS> dispatch_q;
    std::deque<RS> sched_q;

    int num_regs = 128;
    std::vector<Register> reg_file;

    std::vector<Instruction>& instructions;
    int ip; // Instruction pointer

    // Pipeline stages
    int fetch();
    void dispatch();
    void schedule();
    void execute();
    void update();

    // Tag generation
    int curr_tag = 0;
    inline int get_tag() {
        return curr_tag++;
    }
};

#endif
