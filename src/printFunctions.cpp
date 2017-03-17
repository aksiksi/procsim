//Example of how to print the results, but you can also use your own method. 
typedef struct _proc_inst_t
{
        uint32_t instruction_address;
        int32_t op_code;
        int32_t src_reg[2];
        int32_t dest_reg;
        uint32_t target_address;
        int32_t jump_cond;
} proc_inst_t; 


typedef struct _proc_stats_t
{
        unsigned long retired_instruction;
        unsigned long Issue_instruction;
        unsigned long total_disp_size;
        float avg_inst_retired;
        float avg_inst_Issue;
        float avg_disp_size;
        unsigned long max_disp_size;
        unsigned long cycle_count;
} proc_stats_t;

void print_statistics(proc_stats_t* p_stats, FILE* fp) {
        fprintf(fp,"Processor stats:\n");
        fprintf(fp,"Total instructions: %lu\n", p_stats->retired_instruction);
        fprintf(fp,"Avg Dispatch queue size: %f\n", p_stats->avg_disp_size);
        fprintf(fp,"Maximum Dispatch queue size: %lu\n", p_stats->max_disp_size);
        fprintf(fp,"Avg inst Issue per cycle: %f\n", p_stats->avg_inst_Issue);
        fprintf(fp,"Avg inst retired per cycle: %f\n", p_stats->avg_inst_retired);
        fprintf(fp,"Total run time (cycles): %lu\n", p_stats->cycle_count);
} 

void complete_proc(proc_stats_t *p_stats, FILE* fp)
{
        p_stats->avg_disp_size = (double) p_stats->total_disp_size / p_stats->cycle_count;
        p_stats->avg_inst_retired = (double) p_stats->retired_instruction / p_stats->cycle_count;
        p_stats->avg_inst_Issue = (double) p_stats->Issue_instruction / p_stats->cycle_count;

        fprintf(fp, "INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\n");
        for (int i = 0; i != procSim.Statitics.size(); ++i) {
                fprintf(fp, "%d\t", i + 1);
                for (int j = 0; j != 5; ++j)
                        fprintf(fp, "%d\t", procSim.Statitics[i][j]);
                fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
}          
