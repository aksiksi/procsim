#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <deque>
#include <vector>

struct PipelineOptions {
    int F, J, K, L, R;
};

// Single instruction as parsed from trace file
struct Instruction {
    int idx;
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
    bool empty = true;
    int fu_type;
    int dest_reg;
    int dest_tag;
    bool src1_ready;
    int src1_tag = -1;
    int src1_value = -1;
    bool src2_ready;
    int src2_tag = -1;
    int src2_value = -1;
};

struct ResultBus {
    int fu_id = -1;
    bool busy = false;
    int value, tag, reg_no;
};

struct FU {
    int id; // Uniquely identifies a FU in the table
    int type;
    int value = -1, dest = -1, tag = -1;
    bool busy = false;
    uint64_t start_cycle; // Cycle at which the FU started
};

// Register as stored in register file
struct Register {
    int num, tag, value;
    bool ready;
};

class Pipeline {
public:
    Pipeline(std::vector<Instruction>& ins, PipelineOptions& opt);

    void start();
    void flush();

private:
    uint64_t clock;
    PipelineOptions options;

    // Tomasulo data structures
    std::deque<Instruction> dispatch_q;
    void dispatch_delete(Instruction inst);

    std::vector<RS> sched_q;
    void schedq_insert(Instruction& inst, RS& rs);

    std::vector<ResultBus> result_buses;
    int rb_find_tag(int tag); // Returns ResultBus id which is broadcasting this tag, or -1

    std::vector<FU> fu_table;
    int find_fu(int type);

    int num_regs = 128;
    std::vector<Register> reg_file;

    std::vector<Instruction>& instructions;
    int ip; // Instruction pointer

    // Init the pipeline
    void init();

    // Pipeline "stages"

    // 1. Fetch unit
    int fetch();

    // 2. Dispatch unit
    void dispatch();

    // 3. Scheduling unit
    void check_buses();
    void wake_up();

    // 4. Execution unit
    void execute();
    void update();

    // 5. State update unit
    void update_reg_file();
    void delete_completed();

    // Tag generation
    int curr_tag = 0;
    inline int get_tag() {
        return curr_tag++;
    }
};

#endif
