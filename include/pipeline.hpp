#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <deque>
#include <vector>

enum Stage {
    FETCH,
    DISP,
    SCHED,
    EXEC,
    UPDATE
};

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

// Store status of an instruction as it goes through pipeline
struct InstStatus {
    int num;
    Stage stage;

    // Clock cycle at which instruction entered stage
    uint64_t fetch, disp, sched, exec, state;

    // Update status of the instruction
    void update(uint64_t clock, Stage next) {
        stage = next;

        switch (next) {
            case FETCH:
                fetch = clock;
                break;
            case DISP:
                disp = clock;
                break;
            case SCHED:
                sched = clock;
                break;
            case EXEC:
                exec = clock;
                break;
            case UPDATE:
                state = clock;
                break;
        }
    }
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
};

struct FU {
    int id; // Uniquely identifies a FU in the table
    int type;
    int value = -1, dest = -1, tag = -1;
    int inst_idx;
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

    std::deque<Instruction> dispatch_q;

    std::vector<RS> sched_q;
    bool schedq_empty();
    void schedq_insert(Instruction& inst, RS& rs);
    void sort_schedq(std::deque<RS>& sorted);

    std::vector<ResultBus> result_buses;
    int rb_find_tag(int tag); // Returns ResultBus id which is broadcasting this tag, or -1

    std::vector<FU> fu_table;
    int find_fu(int type);

    int num_regs = 128;
    std::vector<Register> reg_file;

    std::vector<Instruction>& instructions;
    int ip; // Instruction pointer

    std::vector<InstStatus> status;
    bool check_latch(InstStatus& is);

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
