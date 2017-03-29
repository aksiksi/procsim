#include <algorithm>
#include <iostream>

#include "pipeline.hpp"

Pipeline::Pipeline(std::vector<Instruction>& ins, PipelineOptions& opt)
        : options(opt), instructions(ins) {}

void Pipeline::init() {
    // Init IP and clock
    ip = 0;
    clock = 0;
    int i, j;

    // Setup FU table
    int id = 0;
    FU fu;

    // Number of FUs of each type
    int fu_counts[] = {options.J, options.K, options.L};

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

    // Initialize the register file
    for (int i = 0; i < num_regs; i++) {
        reg_file.push_back({i, -1, -1, true, true});
    }

    // Pre-allocate memory for instruction storage
    status.reserve(instructions.size());

    // Resize the sched queue according to given params
    int q_size = 2 * (options.J + options.K + options.L);
    sched_q.resize(q_size);

    // Init stats
    proc_stats = {};
    proc_stats.total_instructions = instructions.size();
    proc_stats.avg_inst_issue += instructions.size();

    // Init predictor with 128 entries and 8 Smith counters per entry (3-bit GHR)
    int n = 128;
    int k = 3;
    predictor = new BranchPredictor(n, k);
}

void Pipeline::start() {
    init();

    // Pipeline loop (single cycle per iteration)
    while (num_completed < instructions.size()) {
        // Retire any completed instructions (remove from schedq)
        proc_stats.avg_inst_retired += retire();

        // Check result buses for broadcasts
        check_buses();

        // Update reg file and free RBs
        state_update();

        // Move FU results on RBs and free up FUs
        execute();

        // Mark independent inst. in sched queue for firing
        wake_up();

        // Dispatch unit reserves slot in sched queue
        dispatch();

        // Fetch inst. into dispatch queue (if instructions available!)
        ip += fetch();

        // Record dispatch queue size
        if (dispatch_q.size() > proc_stats.max_disp_size)
            proc_stats.max_disp_size = dispatch_q.size();

        proc_stats.avg_disp_size += dispatch_q.size();

        clock++;
    }

    clock--;

    // Collect stats
    proc_stats.cycle_count = clock;
    proc_stats.avg_disp_size /= clock;
    proc_stats.avg_inst_issue /= clock;
    proc_stats.avg_inst_retired /= clock;

    // Cleanup
    delete predictor;
}

int Pipeline::fetch() {
    /*
     * Fetch F instructions in dispatch queue every cycle.
     * Stop if all instructions have been fetched.
     */
    int count = 0;

    for (int i = ip; i < (ip + options.F) && i < instructions.size(); i++) {
        // Mispredict mode; keep fetching garbage
        if (mp != Misprediction::NONE) {
            // Fetch 4 instructions per cycle
            fetch_garbage(options.F, mp == Misprediction::TAKEN);
            break;
        }

        // Insert instruction into dispatch queue
        Instruction& inst = instructions[i];
        dispatch_q.push_back(inst);

        // Create a InstStatus entry to track instruction progress
        InstStatus is = {};
        is.idx = num_fetched++;
        is.fetch = clock;
        is.disp = clock+1;
        is.stage = Stage::DISP;
        status.push_back(is);

        // Allocate instruction into ROB
        ROBEntry re = {};
        re.ip = i;
        re.branch = false;
        re.inst_idx = is.idx;
        re.complete = false;

        if (inst.branch_addr == -1)
            rob.push_front(re);

        // Perform branch prediction (since it's a branch!)
        else {
            bool taken = predictor->predict(inst.branch_addr);

            // Store prediction and correct result
            re.branch = true;
            re.p_taken = taken;
            re.taken = inst.taken;

            rob.push_front(re);

            // If wrong, fetch garbage instructions and track status
            if (taken != inst.taken) {
                int c = (ip + options.F) - (i + 1); // Number of garbage inst. to fetch
                fetch_garbage(c, taken);

                // Track status of pipeline
                if (taken)
                    mp = Misprediction::TAKEN;
                else
                    mp = Misprediction::NOT_TAKEN;

                break;
            }
        }

        count++;
    }

    return count;
}

void Pipeline::fetch_garbage(int count, bool taken) {
    for (int i = 0; i < count; i++) {
        // Create a dummy instruction and dispatch it
        Instruction inst = {};
        inst.dest_reg = -1;
        inst.addr = -1;
        inst.src_reg[0] = -1;
        inst.src_reg[1] = -1;
        inst.idx = ++num_fetched;

        // FU depends on prediction
        if (taken)
            inst.fu_type = 1;
        else
            inst.fu_type = 2;

        dispatch_q.push_back(inst);

        // Track the status of this dummy instruction
        InstStatus is = {};
        is.idx = num_fetched++;
        is.fetch = clock;
        is.disp = clock+1;
        is.stage = Stage::DISP;
        status.push_back(is);

        // Allocate to ROB
        ROBEntry re = {};
        re.ip = -1;
        re.branch = false;
        re.inst_idx = is.idx;
        re.complete = false;
        rob.push_front(re);

        i++;
    }
}

void Pipeline::schedq_insert(Instruction& inst, RS& rs) {
    /* Insert an Instruction into the schedQ */
    rs.fu_type = inst.fu_type;
    rs.dest_reg = inst.dest_reg;
    rs.inst_idx = inst.idx;

    int src1 = inst.src_reg[0];
    int src2 = inst.src_reg[1];

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
        rs.src1_tag = -1;
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
        rs.src2_tag = -1;
    }

    rs.empty = false;
}

void Pipeline::dispatch() {
    int dispatched = 0;
    int i;
    int rs_idx;

    for (i = 0; i < dispatch_q.size(); i++) {
        Instruction& inst = dispatch_q[i];

        rs_idx = 0;

        for (RS& rs: sched_q) {
            // Add instruction to first free slot in schedQ
            if (rs.empty) {
                schedq_insert(inst, rs);

                // Always generate a new tag for dest, even if -1!
                int dest = inst.dest_reg;
                rs.dest_tag = get_tag();

                if (dest != -1) {
                    reg_file[dest].tag = rs.dest_tag;
                    reg_file[dest].ready = false;
                }

                // Add to SCHED stage
                InstStatus& is = status[inst.idx];
                is.sched = clock+1;
                is.stage = Stage::SCHED;

                // Create pipeline entry and add to pipeline
                PipelineEntry pe = {};
                pe.inst_idx = inst.idx;
                pe.rs_idx = rs_idx;
                pe.cycle = clock+1;
                pe.tag = rs.dest_tag;

                stages.sched.push_back(pe);

                dispatched++;

                break;
            }

            rs_idx++;
        }
    }

    // Delete all dispatched instructions
    for (i = 0; i < dispatched; i++)
        dispatch_q.pop_front();
}

int Pipeline::rb_find_tag(int tag) {
    /*
     * Find if particular tag is one a result bus
     * If so, return index of RB; otherwise, -1
     */
    ResultBus rb;

    for (int i = 0; i < result_buses.size(); i++) {
        rb = result_buses[i];
        if (rb.busy && rb.tag == tag)
            return i;
    }

    return -1;
}

void Pipeline::check_buses() {
    for (RS& rs: sched_q) {
        // Skip empty entries
        if (rs.empty)
            continue;

        int tags[] = {rs.src1_tag, rs.src2_tag};
        int i = 0;

        for (int tag: tags) {
            // Check for a broadcast on a result bus for this tag
            if (tag != -1) {
                // Find idx of the ResultBus
                int rb_idx = rb_find_tag(tag);

                // Broadcast found
                if (rb_idx != -1) {
                    ResultBus& rb = result_buses[rb_idx];

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

void Pipeline::wake_up() {
    std::vector<PipelineEntry> sorted;
    sort_stage(sorted, Stage::SCHED);

    for (PipelineEntry& pe: sorted) {
        RS& rs = sched_q[pe.rs_idx];

        if (rs.src1_ready && rs.src2_ready) {
            int fu_type = rs.fu_type;
            int fu_idx = find_fu(fu_type);

            // Found a free FU of the given type
            if (fu_idx != -1) {
                // Issue the instruction
                FU& fu = fu_table[fu_idx];
                fu.inst_idx = rs.inst_idx;
                fu.dest = rs.dest_reg;
                fu.tag = rs.dest_tag;
                fu.busy = true;

                // Advance to EXEC stage
                InstStatus& is = status[pe.inst_idx];
                is.exec = clock+1;
                is.stage = Stage::EXEC;

                pe.cycle = clock+1;
                stages.exec.push_back(pe);

                // Remove from SCHED stage
                stages.sched.remove_if([pe](PipelineEntry p1) {
                    return pe.inst_idx == p1.inst_idx;
                });

                // Mark instruction as complete in ROB
                // Idea: Iterate through ROB in reverse order, and look for matching idx
                for (uint64_t i = rob.size(); i > 0; i--) {
                    ROBEntry& re = rob[i-1];
                    if (re.inst_idx == pe.inst_idx) {
                        re.complete = true;
                        break;
                    }
                }
            }
        }
    }
}

int Pipeline::find_fu_by_tag(int tag) {
    for (FU& fu: fu_table) {
        if (fu.tag == tag)
            return fu.id;
    }

    return -1;
}

void Pipeline::sort_stage(std::vector<PipelineEntry>& l, Stage s) {
    std::list<PipelineEntry>* stage;

    if (s == Stage::SCHED)
        stage = &stages.sched;
    else if (s == Stage::EXEC)
        stage = &stages.exec;
    else if (s == Stage::UPDATE)
        stage = &stages.update;
    else
        return;

    for (PipelineEntry& pe: *stage) {
        if (clock >= pe.cycle)
            l.push_back(pe);
    }

    if (!l.empty()) {
        std::sort(l.begin(), l.end(), [](const PipelineEntry& p1, const PipelineEntry& p2) {
            if (p1.cycle == p2.cycle)
                return p1.tag < p2.tag;
            else
                return p1.cycle < p2.cycle;
        });
    }
}

void Pipeline::execute() {
    std::vector<PipelineEntry> sorted;
    sort_stage(sorted, Stage::EXEC);

    // Start from ROB end ("head")
    // If instruction is complete, put onto a RB
    // If instruction is a branch, check if prediction correct
    // If prediction wrong, flush EVERYTHING

    // Track number of instructions to remove from ROB
    int num_done = 0;

    for (auto it = rob.rend(); it != rob.rbegin(); ++it) {
        // Stop checking ROB if instruction is not complete yet
        ROBEntry& re = *it;
        if (!re.complete)
            break;

        for (PipelineEntry& pe: sorted) {
            // Find correct entry in pipeline
            if (pe.inst_idx != re.inst_idx)
                continue;

            RS& rs = sched_q[pe.rs_idx];

            // Find a free RB
            for (ResultBus &rb: result_buses) {
                // Assign result from FU to a free RB
                if (!rb.busy) {
                    rb.busy = true;
                    rb.tag = rs.dest_tag;
                    rb.value = -1; // Don't really care about value from FU!
                    rb.reg_no = rs.dest_reg;
                    rb.inst_idx = rs.inst_idx;

                    // Free up the FU
                    int fu_idx = find_fu_by_tag(rs.dest_tag);
                    fu_table[fu_idx].busy = false;
                    rb.fu_id = fu_idx;

                    // Advance to UPDATE stage
                    InstStatus& is = status[pe.inst_idx];
                    is.state = clock+1;
                    is.stage = Stage::UPDATE;

                    pe.cycle = clock+1;
                    stages.update.push_back(pe);

                    // Remove from EXEC stage
                    stages.exec.remove_if([pe](PipelineEntry p1) {
                        return pe.inst_idx == p1.inst_idx;
                    });

                    num_done++;

                    break;
                }
            }
        }

        if (re.branch) {
            // Check if prediction incorrect
            if (re.taken != re.p_taken) {
                // Flush everything from DISP and clear the queue
                for (Instruction& i: dispatch_q)
                    status[i.idx].disp = 0;

                dispatch_q.clear();

                // Flush the EXEC and SCHED and mark that in InstStatus
                for (PipelineEntry& pe: stages.sched)
                    status[pe.inst_idx].sched = 0;

                stages.sched.clear();

                for (PipelineEntry& pe: stages.exec)
                    status[pe.inst_idx].exec = 0;

                stages.exec.clear();

                // Empty the sched queue
                for (RS& rs: sched_q) {
                    if (rs.inst_idx > re.inst_idx)
                        rs.empty = true;
                }
            }

            // Update GHR entry in predictor
            // TODO
        }
    }

    // Remove completed instructions
    for (int i = 0; i < num_done; i++)
        rob.pop_back();

//    for (PipelineEntry& pe: sorted) {
//        RS& rs = sched_q[pe.rs_idx];
//
//        // Find a free RB
//        for (ResultBus &rb: result_buses) {
//            // Assign result from FU to a free RB
//            if (!rb.busy) {
//                rb.busy = true;
//                rb.tag = rs.dest_tag;
//                rb.value = -1; // Don't really care about value from FU!
//                rb.reg_no = rs.dest_reg;
//                rb.inst_idx = rs.inst_idx;
//
//                // Free up the FU
//                int fu_idx = find_fu_by_tag(rs.dest_tag);
//                fu_table[fu_idx].busy = false;
//                rb.fu_id = fu_idx;
//
//                // Advance to UPDATE stage
//                InstStatus& is = status[pe.inst_idx];
//                is.state = clock+1;
//                is.stage = Stage::UPDATE;
//
//                pe.cycle = clock+1;
//                stages.update.push_back(pe);
//
//                // Perform flush of ROB if needed!
//                // Don't forget to set misprediction = NONE
//
//                // Remove from EXEC stage
//                stages.exec.remove_if([pe](PipelineEntry p1) {
//                    return pe.inst_idx == p1.inst_idx;
//                });
//
//                break;
//            }
//        }
//    }
}

void Pipeline::state_update() {
    std::vector<PipelineEntry> copy;
    for (PipelineEntry& pe: stages.update)
        copy.push_back(pe);

    for (PipelineEntry& pe: copy) {
        RS& rs = sched_q[pe.rs_idx];

        int rb_idx = rb_find_tag(rs.dest_tag);

        if (rb_idx != -1) {
            ResultBus& rb = result_buses[rb_idx];

            // Look for broadcasted tag in reg_file; otherwise, it's a dest_reg = -1
            if (rb.reg_no != -1) {
                Register& reg = reg_file[rb.reg_no];

                if (reg.tag == rb.tag) {
                    reg.ready = true;
                    reg.value = rb.value;
                }
            }

            rb.busy = false;

            // Move to RETIRE stage
            status[pe.inst_idx].stage = Stage::RETIRE;
            pe.cycle = clock+1;
            stages.retire.push_back(pe);

            // Remove from UPDATE
            stages.update.remove_if([pe](PipelineEntry p1) {
                return pe.inst_idx == p1.inst_idx;
            });
        }
    }
}

int Pipeline::retire() {
    std::vector<PipelineEntry> copy;
    for (PipelineEntry& pe: stages.retire)
        copy.push_back(pe);

    uint64_t prev_completed = num_completed;

    for (PipelineEntry& pe: copy) {
        // Remove instruction from schedq
        RS& rs = sched_q[pe.rs_idx];
        rs.empty = true;

        // Mark as completed
        status[pe.inst_idx].stage = Stage::DONE;

        // Remove from RETIRE
        stages.retire.remove_if([pe](PipelineEntry p1) {
            return pe.inst_idx == p1.inst_idx;
        });

        num_completed++;
    }

    return static_cast<int>(num_completed - prev_completed);
}

void Pipeline::flush_rob() {
    // Flush ROB on branch misprediction
}
