#include <iostream>

#include "pipeline.hpp"

Pipeline::Pipeline(std::vector<Instruction>& ins, PipelineOptions& opt) : instructions(ins) {
    options = opt;

    init();
}

void Pipeline::init() {
    clock = 0;
    int i, j;

    // Setup FU table
    int id = 0;
    FU fu;

    // Number of FUs of each type
    int fu_counts[3] = {options.J, options.K, options.L};

    for (i = 0; i < 3; i++) {
        for (j = 0; j < fu_counts[i]; j++) {
            fu = {};
            fu.id = id++;
            fu.type = i;
            fu_table.push_back(fu);
        }
    }

    // Setup result buses
    ResultBus rb;

    for (i = 0; i < options.R; i++) {
        rb = {};
        result_buses.push_back(rb);
    }

    // Resize the sched queue according to given params
    int q_size = 2 * (options.J + options.K + options.L);
    sched_q.resize(q_size);
}

void Pipeline::start() {
    ip = 0;

    // Initialize the register file
    for (int i = 0; i < num_regs; i++)
        reg_file.push_back({i, -1, -1, true});

    // Pipeline loop (single cycle per iteration)
    while (!sched_q.empty()) {
        ///// Phase 1
        // Reg file written via results bus
        update_reg_file();

        // Mark independent inst. in sched queue for firing
        wake_up();

        // Dispatch unit reserves slot in sched queue
        dispatch();

        ///// Phase 2
        // Fetch inst. into dispatch queue (if instructions available!)
        ip += fetch();

        // Sched queue updated via result bus
        check_buses();

        // State update deletes inst. from sched queue
        delete_completed();

        clock++;
    }
}

int Pipeline::fetch() {
    /*
     * Fetch F instructions in dispatch queue every cycle.
     * Stop if all instructions have been fetched.
     */
    int count = 0;

    for (int i = ip; i < (ip + options.F) && i < instructions.size(); i++) {
        Instruction inst = instructions[i];
        dispatch_q.push_back(inst);
        count++;
    }

    return count;
}

void Pipeline::schedq_insert(Instruction& inst, RS& rs) {
    rs.fu_type = inst.fu_type;
    rs.dest_reg = inst.dest_reg;
    rs.dest_tag = get_tag();

    int src1 = inst.src1_reg;
    int src2 = inst.src2_reg;

    if (src1 != -1) {
        if (reg_file[src1].ready) {
            rs.src1_value = reg_file[src1].value;
            rs.src1_ready = true;
        } else {
            rs.src1_tag = reg_file[src1].tag;
            rs.src1_ready = false;
        }
    }

    if (src2 != -1) {
        if (reg_file[src2].ready) {
            rs.src2_value = reg_file[src2].value;
            rs.src2_ready = true;
        } else {
            rs.src2_tag = reg_file[src2].tag;
            rs.src2_ready = false;
        }
    }

    rs.empty = false;
}

void Pipeline::dispatch_delete(Instruction inst) {
    // http://stackoverflow.com/questions/13088297/erase-elements-in-a-vector-of-struct-according-to-a-property-value
    // Uses a C++11 lambda for simplicity
    // Note that contents of `[]` define what is to be captured into lambda
    const int idx = inst.idx;
    auto remove = std::remove_if(dispatch_q.begin(), dispatch_q.end(),
                                 [&idx](const Instruction& i) { return i.idx == idx; });
    dispatch_q.erase(remove, dispatch_q.end());
}

void Pipeline::dispatch() {
    for (int i = 0; i < dispatch_q.size(); i++) {
        Instruction inst = dispatch_q[i];

        for (RS& rs: sched_q) {
            // Add instruction to first free slot in schedQ
            if (rs.empty) {
                schedq_insert(inst, rs);

                int dest = inst.dest_reg;

                if (dest != -1) {
                    reg_file[dest].tag = get_tag();
                    rs.dest_tag = reg_file[dest].tag;
                    reg_file[dest].ready = false;
                }

                // Delete instruction from dispatch queue
//                dispatch_delete(inst);
                dispatch_q.pop_front();

                break;
            }
        }
    }
}

int Pipeline::rb_find_tag(int tag) {
    ResultBus rb;

    for (int i = 0; i < result_buses.size(); i++) {
        rb = result_buses[i];
        if (rb.tag == tag)
            return i;
    }

    return -1;
}

void Pipeline::check_buses() {
    for (RS& rs: sched_q) {
        // Check for a broadcast on a result bus for this tag
        if (rs.src1_tag != -1) {
            // Find idx of the ResultBus
            int rb_idx = rb_find_tag(rs.src1_tag);

            // Broadcast found
            if (rb_idx != -1) {
                // Update RS with result from bus
                ResultBus rb = result_buses[rb_idx];
                rs.src1_ready = true;
                rs.src1_value = rb.value;
            }
        }
    }
}

int Pipeline::find_fu(int type) {
    // Returns a free FU of a given type, -1 if not found
    if (type == -1) type = 1;

    for (FU& fu: fu_table) {
        if (fu.type == type && !fu.busy)
            return fu.id;
    }

    return -1;
}

void Pipeline::wake_up() {
    for (RS& rs: sched_q) {
        // TODO: sort by tag number for firing!
        if (rs.src1_ready && rs.src2_ready) {
            int fu_type = rs.fu_type;
            int fu_idx = find_fu(fu_type);

            // Found a free FU of the given type
            if (fu_idx != -1) {
                // Issue the instruction
                FU fu = fu_table[fu_idx];
                fu.start_cycle = clock;
                fu.busy = true;
            }
        }
    }
}

void Pipeline::execute() {
    // Update a ResultBus if execution of FU complete
    for (FU& fu: fu_table) {
        // Done executing
        if (fu.start_cycle + 1 == clock) {
            // Find a free ResultBus
            for (ResultBus& rb: result_buses) {
                if (!rb.busy) {
                    rb.busy = true;
                    rb.tag = fu.tag;
                    rb.value = fu.value;
                    rb.reg_no = fu.dest;
                    fu.busy = false;
                }
            }
        }
    }

}

void Pipeline::update_reg_file() {
    for (int i = 0; i < reg_file.size(); i++) {
        Register reg = reg_file[i];

        for (ResultBus& rb: result_buses) {
            int rb_idx = rb_find_tag(reg.tag);

            if (rb.busy && rb.reg_no == reg.num && rb_idx != -1) {
                reg_file[i].ready = true;
                reg_file[i].value = rb.value;
                rb.busy = false;
            }
        }
    }
}

void Pipeline::delete_completed() {
    for (RS& rs: sched_q) {
        // Found an instruction that's complete, mark it as removable from queue
        if (rs.src1_ready && rs.src2_ready && !rs.empty)
            rs.empty = true;
    }
}
