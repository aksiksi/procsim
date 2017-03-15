#include "pipeline.hpp"

Pipeline::Pipeline(PipelineOptions& opt) {
    clock = 0;
    options = opt;

    // Resize the sched queue
    int q_size = 2 * (options.J + options.K + options.L);
    sched_q.resize(q_size);
}

void Pipeline::start() {
    ip = 0;

    // Initialize the register file
    for (int i = 0; i < num_regs; i++)
        reg_file.push_back({i, -1, -1});

    // Pipeline loop (single cycle per iteration)
    while (ip < instructions.size() && !sched_q.empty()) {
        // Fetch some inst. into dispatch
        ip += fetch();

        // Reg file written via results bus

        // Mark independent inst. in sched queue for firing

        // Dispatch unit reserves slot in sched queue
        dispatch();

        // Reg file read by dispatch

        // Sched queue updated via result bus

        // State update deletes inst. from sched queue
    }
}

int Pipeline::fetch() {
    /* Fetch F instructions in dispatch queue every cycle. */
    for (int i = ip; i < (ip + options.F); i++) {
        Instruction inst = instructions[i];

        // TODO: fill this in
        RS rs = {};
        rs.fu_type = inst.fu_type;
        rs.dest_reg = inst.dest_reg;
        rs.dest_tag = get_tag();
    }

    return options.F;
}

void Pipeline::dispatch(){

}

void Pipeline::schedule() {

}

void Pipeline::execute() {

}

void Pipeline::update() {

}
