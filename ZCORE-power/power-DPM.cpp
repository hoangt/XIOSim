/* power-DPM.cpp - Power proxy for OoO core */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(power_opt_string,"DPM"))
    return new core_power_DPM_t(core);
#else

class core_power_DPM_t : public core_power_t {

  public:
  core_power_DPM_t(struct core_t *_core);

  void translate_params(system_core *core_params, system_L2 *L2_params);
  void translate_stats(struct stat_sdb_t* sdb, system_core* core_stats, system_L2 *L2_stats);
};

core_power_DPM_t::core_power_DPM_t(struct core_t *_core):
  core_power_t(_core)
{
}

void core_power_DPM_t::translate_params(system_core *core_params, system_L2 *L2_params)
{
  core_power_t::translate_params(core_params, L2_params);

  struct core_knobs_t *knobs = core->knobs;
  core_params->machine_type = 0; // OoO
  core_params->number_hardware_threads = 2;
  core_params->number_instruction_fetch_ports = 2;
  core_params->fp_issue_width = 2;
  core_params->prediction_width = 1;
  core_params->pipelines_per_core[0] = 1;
  core_params->pipelines_per_core[1] = 1;
  core_params->pipeline_depth[0] = 15;
  core_params->pipeline_depth[1] = 15;

  core_params->instruction_window_scheme = 1; //RSBASED 0; // PHYREG
  core_params->instruction_window_size = knobs->exec.RS_size;
  core_params->archi_Regs_IRF_size = 16;
  core_params->archi_Regs_FRF_size = 32;
  core_params->phy_Regs_IRF_size = 256;
  core_params->phy_Regs_FRF_size = 256;
  core_params->rename_scheme = 0; //RAM-based
  core_params->register_windows_size = 0;
  strcpy(core_params->LSU_order, "inorder");
  core_params->memory_ports = 2;

  // private L2 data cache
  if (core->memory.DL2)
  {
    zesto_assert(L2_params != NULL, (void)0);

    L2_params->L2_config[0] = core->memory.DL2->sets * core->memory.DL2->assoc * core->memory.DL2->linesize;
    L2_params->L2_config[1] = core->memory.DL2->linesize;
    L2_params->L2_config[2] = core->memory.DL2->assoc;
    L2_params->L2_config[3] = core->memory.DL2->banks;
    L2_params->L2_config[4] = 1;
    L2_params->L2_config[5] = core->memory.DL2->latency;
    L2_params->L2_config[6] = core->memory.DL2->bank_width;
    L2_params->L2_config[7] = (core->memory.DL2->write_policy == WRITE_THROUGH) ? 0 : 1;

    L2_params->device_type = XML->sys.device_type;

    L2_params->ports[0] = 1;
    L2_params->ports[1] = 0;
    L2_params->ports[2] = 0;

    L2_params->buffer_sizes[0] = 1;
    L2_params->buffer_sizes[1] = 2;
    L2_params->buffer_sizes[2] = 2;
    L2_params->buffer_sizes[3] = 2;
  }
}

void core_power_DPM_t::translate_stats(struct stat_sdb_t* sdb, system_core *core_stats, system_L2 *L2_stats)
{
  core_power_t::translate_stats(sdb, core_stats, L2_stats);

  struct stat_stat_t *stat;
  int coreID = core->id;

  stat = stat_find_core_stat(sdb, coreID, "fetch_uops");
  core_stats->int_instructions = stat->variant.for_sqword.end_val; 
  core_stats->fp_instructions = 0;

  stat = stat_find_core_stat(sdb, coreID, "commit_uops");
  core_stats->committed_int_instructions = stat->variant.for_sqword.end_val;
  core_stats->committed_fp_instructions = 0;

  stat = stat_find_core_stat(sdb, coreID, "commit_uops");
  core_stats->ROB_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "ROB_writes");
  core_stats->ROB_writes = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "regfile_reads");
  core_stats->rename_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "regfile_writes");
  core_stats->rename_writes = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "fp_regfile_reads");
  core_stats->fp_rename_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "fp_regfile_writes");
  core_stats->fp_rename_writes = stat->variant.for_sqword.end_val;

  stat = stat_find_core_stat(sdb, coreID, "alloc_uops");
  core_stats->inst_window_reads = stat->variant.for_sqword.end_val;
  stat = stat_find_core_stat(sdb, coreID, "alloc_uops");
  core_stats->inst_window_writes = stat->variant.for_sqword.end_val;
  core_stats->inst_window_wakeup_accesses = 0;
  core_stats->fp_inst_window_reads = 0;
  core_stats->fp_inst_window_writes = 0;
  core_stats->fp_inst_window_wakeup_accesses = 0;

  stat = stat_find_core_stat(sdb, coreID, "oracle_total_calls");
  core_stats->context_switches = stat->variant.for_sqword.end_val;

  if (core->memory.DL2)
  {
    zesto_assert(L2_stats != NULL, (void)0);

    stat = stat_find_core_stat(sdb, coreID, "DL2.load_lookups");
    L2_stats->read_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL2.load_misses");
    L2_stats->read_misses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL2.store_lookups");
    L2_stats->write_accesses = stat->variant.for_sqword.end_val;
    stat = stat_find_core_stat(sdb, coreID, "DL2.store_misses");
    L2_stats->write_misses = stat->variant.for_sqword.end_val;
  }
}
#endif
