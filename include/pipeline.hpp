#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <deque>
#include <list>
#include <vector>

#include "predictor.hpp"

struct Stats {
    uint64_t total_instructions;
    uint64_t total_disp_size;
    double avg_inst_retired;
    double avg_inst_issue;
    double avg_disp_size;
    uint64_t max_disp_size;
    uint64_t cycle_count;
};

enum Stage {
    FETCH,
    DISP,
    SCHED,
    EXEC,
    UPDATE,
    RETIRE,
    DONE
};

struct ROBEntry {
    int inst_idx;
    int ip; // Instruction pointer
    int tag, value, dest_reg;
    int address;
    bool branch, taken, p_taken;
    bool complete;
};

enum Misprediction {
    NONE,
    TAKEN,
    NOT_TAKEN
};

struct PipelineOptions {
    int F, J, K, L, R;
};

struct PipelineEntry {
    int tag;
    uint64_t cycle;
    int inst_idx;
    int rs_idx;
    bool dummy = false;
};

struct PipelineStages {
    std::list<PipelineEntry> sched;
    std::list<PipelineEntry> exec;
    std::list<PipelineEntry> update;
    std::list<PipelineEntry> retire;
};

// Single instruction as parsed from trace file
struct Instruction {
    int idx;
    int addr;
    int ip;
    int fu_type;
    int dest_reg;
    int src_reg[2];
    int branch_addr = -1;
    bool taken = false; // Actual branch result
    bool p_taken = false; // Predicted branch result
};

// Store status of every instruction in the trace
struct InstStatus {
    int idx;
    Stage stage;
    int inst; // Position of inst. in instructions vector
    bool dummy = false;

    // Clock cycle at which instruction entered stage
    long fetch, disp, sched, exec, state;
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
    int inst_idx;
};

struct ResultBus {
    int fu_id = -1;
    bool busy = false;
    int value, tag, reg_no;
    int inst_idx;
};

struct FU {
    int id; // Uniquely identifies a FU in the table
    int type;
    int value = -1, dest = -1, tag = -1;
    int inst_idx;
    bool busy = false;
};

// Register as stored in register file
struct Register {
    int num, tag, value;
    bool ready;
    bool empty;
};

class Pipeline {
public:
    std::vector<InstStatus> status;
    uint64_t num_completed = 0;

    Stats proc_stats;

    Pipeline(std::vector<Instruction>& ins, PipelineOptions& opt);

    void start();

private:
    uint64_t clock;

    PipelineOptions options;
    PipelineStages stages = {};
    void sort_stage(std::vector<PipelineEntry>& l, Stage s);

    std::deque<Instruction> dispatch_q;
    std::deque<Instruction> fetch_q;

    std::vector<RS> sched_q;
    void schedq_insert(Instruction& inst, RS& rs);
    int schedq_size = 0;

    std::vector<ResultBus> result_buses;
    int rb_find_tag(int tag); // Returns ResultBus id which is broadcasting this tag, or -1

    std::vector<FU> fu_table;
    int find_fu(int type);
    int find_fu_by_tag(int tag);

    int num_regs = 128;
    std::vector<Register> reg_file;

    std::vector<Instruction>& instructions;
    int ip; // Instruction pointer

    // Branch prediction support
    BranchPredictor* predictor;
    Misprediction mp;
    bool branch_flush = false;
    int branch_idx = -1;
    std::deque<int> branches;

    // Reorder buffer
    std::deque<ROBEntry> rob;

    // Init the pipeline
    void init();

    // Pipeline "stages"
    // 1. Fetch unit
    int fetch();
    int num_fetched = 0;

    // 2. Dispatch unit
    void dispatch();

    // 3. Scheduling unit
    void schedule();
    void check_buses();
    void wake_up();

    // 4. Execution unit
    void execute();

    // 5. State update unit
    void state_update();
    int retire();

    // Tag generation
    int curr_tag = 0;
    inline int get_tag() {
        return curr_tag++;
    }
};

#endif
