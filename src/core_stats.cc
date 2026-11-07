#include "core_stats.h"

cpu_stats operator-(cpu_stats lhs, cpu_stats rhs)
{
  lhs.begin_instrs -= rhs.begin_instrs;
  lhs.begin_cycles -= rhs.begin_cycles;
  lhs.end_instrs -= rhs.end_instrs;
  lhs.end_cycles -= rhs.end_cycles;
  lhs.total_rob_occupancy_at_branch_mispredict -= rhs.total_rob_occupancy_at_branch_mispredict;

  lhs.mispredicts_resolved_at_decode -= rhs.mispredicts_resolved_at_decode;
  lhs.mispredicts_resolved_at_execute -= rhs.mispredicts_resolved_at_execute;
  lhs.total_mispredict_latency_decode -= rhs.total_mispredict_latency_decode;
  lhs.total_mispredict_latency_execute -= rhs.total_mispredict_latency_execute;
  for (std::size_t i = 0; i < lhs.mispredict_latency_histogram.size(); ++i) {
    lhs.mispredict_latency_histogram.at(i) -= rhs.mispredict_latency_histogram.at(i);
  }

  lhs.total_branch_types -= rhs.total_branch_types;
  lhs.branch_type_misses -= rhs.branch_type_misses;

  return lhs;
}
