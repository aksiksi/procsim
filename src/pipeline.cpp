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

    // Resize the sched queue according to given params
    int q_size = 2 * (options.J + options.K + options.L);
    sched_q.resize(q_size);
}

void Pipeline::start() {
    init();

    // Initialize the register file
    for (int i = 0; i < num_regs; i++) {
        reg_file.push_back({i, -1, -1, true, true});
    }

    // Pipeline loop (single cycle per iteration)
    while (completed < instructions.size()) {
        // Reg file written via results bus
        // Sched queue updated via result bus
        retire();
        check_buses();
        update_reg_file();

        // Move FU results on RBs
        // Free up FU, and delete from schedq
        state_update();

        // Mark independent inst. in sched queue for firing
        wake_up();

        // Dispatch unit reserves slot in sched queue
        dispatch();

        ///// Phase 2
        // Fetch inst. into dispatch queue (if instructions available!)
        ip += fetch();

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

    for (i = 0; i < dispatch_q.size(); i++) {
        Instruction& inst = dispatch_q[i];

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

void Pipeline::sort_schedq() {
    /* Sort schedq by dest tag (ascending order) */
    sorted_schedq.clear();

    for (RS& rs: sched_q) {
        if (!rs.empty)
            sorted_schedq.push_back(rs);
    }

    struct {
        bool operator()(RS& r1, RS& r2) {
            return r1.dest_tag < r2.dest_tag;
        }
    } RSComparator;

    std::sort(sorted_schedq.begin(), sorted_schedq.end(), RSComparator);
}

void Pipeline::wake_up() {
    if (schedq_empty())
        return;

    std::vector<InstStatus> sorted_inst;
    sort_inst(sorted_inst, Stage::SCHED);

    for (InstStatus& is: sorted_inst) {
        if (clock >= is.sched) {
            // Find RS in question
            int rs_idx = 0;
            for (RS& rs: sched_q) {
                if (rs.inst_idx == is.num)
                    break;

                rs_idx++;
            }

            RS& rs = sched_q[rs_idx];

            if (rs.src1_ready && rs.src2_ready) {
                int fu_type = rs.fu_type;
                int fu_idx = find_fu(fu_type);

                // Found a free FU of the given type
                if (fu_idx != -1) {
                    // Mark it in exec stage at next clock
                    status[is.num].update(clock+1, Stage::EXEC);
//                    is.update(clock+1, Stage::EXEC);

                    // Issue the instruction
                    FU& fu = fu_table[fu_idx];
                    fu.start_cycle = clock;
                    fu.inst_idx = rs.inst_idx;
                    fu.dest = rs.dest_reg;
                    fu.tag = rs.dest_tag;
                    fu.busy = true;
                }
            }
        }
    }

//    // Sort sched_q in tag order before firing
//    sort_schedq();
//
//    // Iterate through sorted schedq for tag order firing
//    for (RS& rs: sorted_schedq) {
//        // TODO: no need to check for 1 cycle since dispatch() after wake_up()
//        InstStatus& is = status[rs.inst_idx];
//
//        // Don't fire an instruction if it's not in SCHED
//        if (is.stage != Stage::SCHED || clock < is.sched)
//            continue;
//
//        if (rs.src1_ready && rs.src2_ready) {
//            int fu_type = rs.fu_type;
//            int fu_idx = find_fu(fu_type);
//
//            // Found a free FU of the given type
//            if (fu_idx != -1) {
//                // Mark it in exec stage at next clock
//                is.update(clock+1, Stage::EXEC);
//
//                // Issue the instruction
//                FU& fu = fu_table[fu_idx];
//                fu.start_cycle = clock;
//                fu.inst_idx = rs.inst_idx;
//                fu.dest = rs.dest_reg;
//                fu.tag = rs.dest_tag;
//                fu.busy = true;
//            }
//        }
//    }
}

//void Pipeline::execute() {
//    // Free FU complete
//    for (FU& fu: fu_table) {
//        if (!fu.busy)
//            continue;
//
//        InstStatus& is = status[fu.inst_idx];
//
//        if (is.stage == Stage::EXEC && is.exec == clock)
//            fu.busy = false;
//    }
//}

int Pipeline::find_fu_by_tag(int tag) {
    for (FU& fu: fu_table) {
        if (fu.tag == tag)
            return fu.id;
    }

    return -1;
}

void Pipeline::sort_inst(std::vector<InstStatus>& st, Stage stage) {
    for (InstStatus& is: status) {
        if (is.stage == stage)
            st.push_back(is);
    }

    std::sort(st.begin(), st.end(), [stage](const InstStatus& i1, const InstStatus& i2) {
        uint64_t t1 = 0, t2 = 0;

        if (stage == Stage::SCHED) {
            t1 = i1.sched;
            t2 = i2.sched;
        } else if (stage == Stage::EXEC) {
            t1 = i1.exec;
            t2 = i2.exec;
        } else if (stage == Stage::UPDATE) {
            t1 = i1.state;
            t2 = i2.state;
        }

        // Sort by tag order if same cycle
        if (t1 == t2)
            return i1.num < i2.num;
        else
            return t1 < t2;
    });
}

void Pipeline::state_update() {
    // Bus allocation should be in EXEC order, followed by tag order
    std::vector<InstStatus> sorted_status;
    sort_inst(sorted_status, Stage::EXEC);

    for (InstStatus& is: sorted_status) {
        if (clock >= is.exec) {
            // Find RS that has this instruction
            int rs_idx = 0;
            for (RS& rs: sched_q) {
                if (rs.inst_idx == is.num)
                    break;

                rs_idx++;
            }

            RS& rs = sched_q[rs_idx];

            // Find a free RB
            for (ResultBus &rb: result_buses) {
                // Assign result from FU to a free RB
                if (!rb.busy) {
                    rb.busy = true;
                    rb.tag = rs.dest_tag;
                    rb.value = -1; // Don't really care about value from FU!
                    rb.reg_no = rs.dest_reg;
                    rb.inst_idx = rs.inst_idx;

//                    // Free up the FU
                    int fu_idx = find_fu_by_tag(rs.dest_tag);
                    fu_table[fu_idx].busy = false;
                    rb.fu_id = fu_idx;

                    // Advance to UPDATE stage
                    status[is.num].update(clock+1, Stage::UPDATE);
//                    is.update(clock + 1, Stage::UPDATE);

                    break;
                }
            }
        }
    }

    // Use sorted schedq for in-tag-order result bus allocation
//    sort_schedq();
//    for (RS& rs: sorted_schedq) {
//        InstStatus& is = status[rs.inst_idx];
//
//        // Only assign instructions at the end of EXEC to a RB
//        if (is.stage == Stage::EXEC && clock >= is.exec) {
//            // Find a free RB
//            for (ResultBus& rb: result_buses) {
//                // Assign result from FU to a free RB
//                if (!rb.busy) {
//                    rb.busy = true;
//                    rb.tag = rs.dest_tag;
//                    rb.value = -1; // Don't really care about value from FU!
//                    rb.reg_no = rs.dest_reg;
//                    rb.inst_idx = rs.inst_idx;
//
//                    // Free up the FU
//                    int fu_idx = find_fu_by_tag(rs.dest_tag);
//                    fu_table[fu_idx].busy = false;
//                    rb.fu_id = fu_idx;
//
//                    // Advance to UPDATE stage
//                    is.update(clock+1, Stage::UPDATE);
//
//                    break;
//                }
//            }
//        }
//    }
}

void Pipeline::update_reg_file() {
    std::vector<InstStatus> sorted_status;
    sort_inst(sorted_status, Stage::UPDATE);

    for (InstStatus& is: sorted_status) {
        if (clock >= is.state) {
            // Find RS that has this instruction
            int rs_idx = 0;
            for (RS& rs: sched_q) {
                if (rs.inst_idx == is.num)
                    break;

                rs_idx++;
            }

            RS& rs = sched_q[rs_idx];

            int rb_idx = rb_find_tag(rs.dest_tag);
            ResultBus& rb = result_buses[rb_idx];

            if (rb_idx != -1) {
                // Look for broadcasted tag in reg_file; otherwise, it's a dest_reg = -1
                if (rb.reg_no != -1) {
                    Register& reg = reg_file[rb.reg_no];

                    if (reg.tag == rb.tag) {
                        reg.ready = true;
                        reg.value = rb.value;
                    }
                }

                rb.busy = false;

                // Retire instruction
                status[is.num].stage = Stage::RETIRE;
            }
        }
    }

    // Use sorted schedQ to perform reg update in tag order
//    sort_schedq();

//    for (RS& rs: sorted_schedq) {
//        InstStatus& is = status[rs.inst_idx];
//
//        if (is.stage != Stage::UPDATE)
//            continue;
//
//        // Find if an RB is broadcasting this tag
//        int rb_idx;
//        rb_idx = rb_find_tag(rs.dest_tag);
//        ResultBus& rb = result_buses[rb_idx];
//
//        if (rb_idx != -1) {
//            // Look for broadcasted tag in reg_file; otherwise, it's a dest_reg = -1
//            if (rb.reg_no != -1) {
//                Register& reg = reg_file[rb.reg_no];
//
//                if (reg.tag == rb.tag) {
//                    reg.ready = true;
//                    reg.value = rb.value;
//                }
//            }
//
//            rb.busy = false;
//
//            // Remove instruction from schedQ
//            for (RS& rs1: sched_q) {
//                if (rs1.dest_tag == rs.dest_tag) {
//                    rs1.empty = true;
//                    break;
//                }
//            }
//        }
//    }
}

void Pipeline::retire() {
    for (InstStatus& is: status) {
        if (is.stage == Stage::RETIRE) {
            // Find RS that has this instruction
            int rs_idx = 0;
            for (RS& rs: sched_q) {
                if (rs.inst_idx == is.num)
                    break;

                rs_idx++;
            }

            RS& rs = sched_q[rs_idx];
            rs.empty = true;

            is.stage = Stage::DONE;

            completed++;
        }
    }
}
