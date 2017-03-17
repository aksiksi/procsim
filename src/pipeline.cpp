#include <iostream>

#include "pipeline.hpp"

Pipeline::Pipeline(std::vector<Instruction>& ins, PipelineOptions& opt)
        : instructions(ins), options(opt) {}

void Pipeline::init() {
    ip = 0;
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
    init();

    // Initialize the register file
    for (int i = 0; i < num_regs; i++)
        reg_file.push_back({i, -1, -1, true});

    // Pipeline loop (single cycle per iteration)
    while (!schedq_empty() || clock <= 1) {
        // TODO: YOU CAN'T JUST EXEC PIPELINE STAGES LIKE THIS
        // NEED LATCHING LOGIC

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
        execute();

        // State update deletes inst. from sched queue
        delete_completed();

        clock++;
    }
}

bool Pipeline::check_latch(InstStatus& is) {
    /* Check if instruction can move to next stage */
    Stage curr = is.stage;

    switch (curr) {
        case DISP:
            return clock > is.disp;
        case SCHED:
            return clock > is.sched;
        case EXEC:
            return clock > is.exec;
        case FETCH:
        case UPDATE:
            return true;
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

        InstStatus is = {};
        is.num = inst.idx;
        is.update(clock, Stage::FETCH);
        is.update(clock+1, Stage::DISP);
        status.push_back(is);

        count++;
    }

    return count;
}

bool Pipeline::schedq_empty() {
    for (RS& rs: sched_q) {
        if (!rs.empty)
            return false;
    }

    return true;
}


void Pipeline::schedq_insert(Instruction& inst, RS& rs) {
    rs.fu_type = inst.fu_type;
    rs.dest_reg = inst.dest_reg;
    rs.inst_idx = inst.idx;

    if (inst.dest_reg != -1)
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
    } else {
        // If no src1, then ready by default
        rs.src1_ready = true;
    }

    if (src2 != -1) {
        if (reg_file[src2].ready) {
            rs.src2_value = reg_file[src2].value;
            rs.src2_ready = true;
        } else {
            rs.src2_tag = reg_file[src2].tag;
            rs.src2_ready = false;
        }
    } else {
        // If no src2, then ready by default
        rs.src2_ready = true;
    }

    rs.empty = false;
}

void Pipeline::dispatch() {
    int dispatched = 0;
    int i;

    for (i = 0; i < dispatch_q.size(); i++) {
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

                // Update status
                status[inst.idx].update(clock+1, Stage::SCHED);

                dispatched++;

                break;
            }
        }
    }

    // Delete all dispatched instructions
    for (i = 0; i < dispatched; i++)
        dispatch_q.pop_front();
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
        // Skip empty entries
        if (rs.empty)
            continue;

        std::vector<int> tags = {rs.src1_tag, rs.src2_tag};
        int i = 0;

        for (int tag: tags) {
            // Check for a broadcast on a result bus for this tag
            if (tag != -1) {
                // Find idx of the ResultBus
                int rb_idx = rb_find_tag(tag);

                // Broadcast found
                if (rb_idx != -1) {
                    ResultBus rb = result_buses[rb_idx];

                    // Update RS with result from bus
                    if (i == 0) {
                        rs.src1_ready = true;
                        rs.src1_value = rb.value;
                    } else {
                        rs.src2_ready = true;
                        rs.src2_value = rb.value;
                    }
                }
            }

            i++;
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

void Pipeline::sort_schedq(std::deque<RS>& sorted) {
    /* Sort schedq by dest tag (ascending order) */
    for (RS& rs: sched_q)
        sorted.push_back(rs);

    struct {
        bool operator()(RS r1, RS r2) {
            return r1.dest_tag < r2.dest_tag;
        }
    } RSComparator;

    std::sort(sorted.begin(), sorted.end(), RSComparator);
}

void Pipeline::wake_up() {
    if (schedq_empty())
        return;

    // Sort sched_q in tag order for firing
    std::deque<RS> sorted;
    sort_schedq(sorted);

    // Iterate through sorted sched_q for tag order firing
    for (RS& rs: sorted) {
        // TODO: no need to check for 1 cycle since dispatch() after wake_up()
        InstStatus is = status[rs.inst_idx];
        if (rs.empty || is.stage != Stage::SCHED)
            continue;

        if (rs.src1_ready && rs.src2_ready) {
            int fu_type = rs.fu_type;
            int fu_idx = find_fu(fu_type);

            // Found a free FU of the given type
            if (fu_idx != -1) {
                // Mark it in sched stage
                is.update(clock+1, Stage::EXEC);

                // Issue the instruction
                FU fu = fu_table[fu_idx];
                fu.start_cycle = clock;
                fu.inst_idx = rs.inst_idx;
                fu.dest = rs.dest_reg;
                fu.tag = rs.dest_tag;
                fu.busy = true;
            }
        }
    }
}

void Pipeline::execute() {
    // Update a ResultBus if execution of FU complete
    // TODO: see if in tag order required!
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
    // Other approach
    // 1. Iterate through each RB
    // 2. See tag being broadcast
    // TODO: see if retiring must be in tag order
    for (int i = 0; i < reg_file.size(); i++) {
        Register reg = reg_file[i];

        for (ResultBus& rb: result_buses) {
            int rb_idx = rb_find_tag(reg.tag);

            if (rb_idx != -1 && rb.busy && rb.reg_no == reg.num) {
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
        if (rs.src1_ready && rs.src2_ready && !rs.empty) {
            // Skip if not fully through the pipeline
            InstStatus is = status[rs.inst_idx];
            if (is.stage != Stage::EXEC)
                continue;

            // Check if dest reg is ready in reg file
            if (rs.dest_reg != -1) {
                if (reg_file[rs.dest_reg].ready)
                    rs.empty = true;
            } else {
                rs.empty = true;
            }
        }
    }
}
