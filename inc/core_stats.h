#ifndef CORE_STATS_H
#define CORE_STATS_H

#include <array>
#include <cstdint>
#include <string>

#include "event_counter.h"
#include "instruction.h"

struct cpu_stats {
  std::string name;
  long long begin_instrs = 0;
  long long begin_cycles = 0;
  long long end_instrs = 0;
  long long end_cycles = 0;
  uint64_t total_rob_occupancy_at_branch_mispredict = 0;

  // ROB occupancy sampled every cycle (average = total_rob_occupancy / rob_occupancy_samples)
  uint64_t total_rob_occupancy = 0;
  uint64_t rob_occupancy_samples = 0;

  // Mispredict resolve latency: cycles from the mispredict (fetch stops) until the branch resolves
  // (fetch resumes BRANCH_MISPREDICT_PENALTY cycles later). Split by where it resolved.
  // Histogram buckets: [0,10) [10,25) [25,50) [50,100) [100,200) [200,inf)
  static constexpr std::array<long long, 5> mispredict_latency_bucket_bounds{10, 25, 50, 100, 200};
  uint64_t mispredicts_resolved_at_decode = 0;
  uint64_t mispredicts_resolved_at_execute = 0;
  uint64_t total_mispredict_latency_decode = 0;
  uint64_t total_mispredict_latency_execute = 0;
  std::array<uint64_t, mispredict_latency_bucket_bounds.size() + 1> mispredict_latency_histogram{};

  champsim::stats::event_counter<branch_type> total_branch_types = {};
  champsim::stats::event_counter<branch_type> branch_type_misses = {};

  [[nodiscard]] auto instrs() const { return end_instrs - begin_instrs; }
  [[nodiscard]] auto cycles() const { return end_cycles - begin_cycles; }
};

cpu_stats operator-(cpu_stats lhs, cpu_stats rhs);

#endif
