#include <catch.hpp>

#include "core_stats.h"
#include "stats_printer.h"

namespace
{
// The per-cycle ROB occupancy and mispredict resolve-latency lines, as printed for a CPU with no samples and no
// resolved mispredicts. They follow the branch prediction line (index 1).
void insert_empty_extended_cpu_lines(std::vector<std::string>& expected, const std::string& name, bool has_cycles)
{
  std::vector<std::string> extra{
      name + " Average ROB Occupancy (per cycle): - over 0 cycles",
      name + " Mispredict Resolve Latency: average - cycles over 0 mispredicts | at decode: 0 (avg - cycles) | at execute: 0 (avg - cycles) | fetch stalled "
             "on mispredicts: "
          + (has_cycles ? "0" : "-") + "% of cycles",
      name + " Mispredict Resolve Latency histogram (cycles): <10: 0 (-%) | 10-24: 0 (-%) | 25-49: 0 (-%) | 50-99: 0 (-%) | 100-199: 0 (-%) | >=200: 0 (-%)"};
  expected.insert(std::next(std::begin(expected), 2), std::begin(extra), std::end(extra));
}
} // namespace

TEST_CASE("An empty core stats prints zero")
{
  cpu_stats given{};
  given.name = "test_cpu";

  std::vector<std::string> expected{"test_cpu cumulative IPC: - instructions: 0 cycles: 0",
                                    "test_cpu Branch Prediction Accuracy: -% MPKI: - Average ROB Occupancy at Mispredict: -",
                                    "Branch type MPKI",
                                    "BRANCH_DIRECT_JUMP: -",
                                    "BRANCH_INDIRECT: -",
                                    "BRANCH_CONDITIONAL: -",
                                    "BRANCH_DIRECT_CALL: -",
                                    "BRANCH_INDIRECT_CALL: -",
                                    "BRANCH_RETURN: -"};

  insert_empty_extended_cpu_lines(expected, given.name, given.cycles() > 0);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The number of instructions and cycles modifies the IPC")
{
  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 100;
  given.end_cycles = 50;

  std::vector<std::string> expected{"test_cpu cumulative IPC: 2 instructions: 100 cycles: 50",
                                    "test_cpu Branch Prediction Accuracy: -% MPKI: 0 Average ROB Occupancy at Mispredict: -",
                                    "Branch type MPKI",
                                    "BRANCH_DIRECT_JUMP: 0",
                                    "BRANCH_INDIRECT: 0",
                                    "BRANCH_CONDITIONAL: 0",
                                    "BRANCH_DIRECT_CALL: 0",
                                    "BRANCH_INDIRECT_CALL: 0",
                                    "BRANCH_RETURN: 0"};

  insert_empty_extended_cpu_lines(expected, given.name, given.cycles() > 0);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The number of mispredictions modifies the MPKI")
{
  auto num_misses = 255;
  auto [line_index, miss_type, expected_line] =
      GENERATE(as<std::tuple<std::size_t, branch_type, std::string>>{}, std::tuple{3, branch_type::BRANCH_DIRECT_JUMP, "BRANCH_DIRECT_JUMP: 255"},
               std::tuple{4, branch_type::BRANCH_INDIRECT, "BRANCH_INDIRECT: 255"}, std::tuple{5, branch_type::BRANCH_CONDITIONAL, "BRANCH_CONDITIONAL: 255"},
               std::tuple{6, branch_type::BRANCH_DIRECT_CALL, "BRANCH_DIRECT_CALL: 255"},
               std::tuple{7, branch_type::BRANCH_INDIRECT_CALL, "BRANCH_INDIRECT_CALL: 255"}, std::tuple{8, branch_type::BRANCH_RETURN, "BRANCH_RETURN: 255"});

  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 500;
  given.total_branch_types.set(miss_type, 2 * num_misses);
  given.branch_type_misses.set(miss_type, num_misses);

  std::vector<std::string> expected{"test_cpu cumulative IPC: 2 instructions: 1000 cycles: 500",
                                    "test_cpu Branch Prediction Accuracy: 50% MPKI: 255 Average ROB Occupancy at Mispredict: 0",
                                    "Branch type MPKI",
                                    "BRANCH_DIRECT_JUMP: 0",
                                    "BRANCH_INDIRECT: 0",
                                    "BRANCH_CONDITIONAL: 0",
                                    "BRANCH_DIRECT_CALL: 0",
                                    "BRANCH_INDIRECT_CALL: 0",
                                    "BRANCH_RETURN: 0"};
  expected.at(line_index) = expected_line;

  insert_empty_extended_cpu_lines(expected, given.name, given.cycles() > 0);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The ROB occupancy modifies the flush penalty")
{
  auto num_misses = 100;

  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 500;
  given.total_branch_types.set(branch_type::BRANCH_DIRECT_JUMP, 2 * num_misses);
  given.branch_type_misses.set(branch_type::BRANCH_DIRECT_JUMP, num_misses);
  given.total_rob_occupancy_at_branch_mispredict = (uint64_t)(10 * num_misses);

  std::vector<std::string> expected{"test_cpu cumulative IPC: 2 instructions: 1000 cycles: 500",
                                    "test_cpu Branch Prediction Accuracy: 50% MPKI: 100 Average ROB Occupancy at Mispredict: 10",
                                    "Branch type MPKI",
                                    "BRANCH_DIRECT_JUMP: 100",
                                    "BRANCH_INDIRECT: 0",
                                    "BRANCH_CONDITIONAL: 0",
                                    "BRANCH_DIRECT_CALL: 0",
                                    "BRANCH_INDIRECT_CALL: 0",
                                    "BRANCH_RETURN: 0"};

  insert_empty_extended_cpu_lines(expected, given.name, given.cycles() > 0);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The per-cycle ROB occupancy is averaged over the sampled cycles")
{
  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 500;
  given.total_rob_occupancy = 1500;
  given.rob_occupancy_samples = 500;

  REQUIRE(champsim::plain_printer::format(given).at(2) == "test_cpu Average ROB Occupancy (per cycle): 3 over 500 cycles");
}

TEST_CASE("Mispredict resolve latency is split by resolution point and binned")
{
  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 1000;
  given.mispredicts_resolved_at_decode = 2;
  given.total_mispredict_latency_decode = 10;
  given.mispredicts_resolved_at_execute = 8;
  given.total_mispredict_latency_execute = 800;
  given.mispredict_latency_histogram = {2, 0, 1, 0, 6, 1};

  auto lines = champsim::plain_printer::format(given);
  REQUIRE(lines.at(3)
          == "test_cpu Mispredict Resolve Latency: average 81 cycles over 10 mispredicts | at decode: 2 (avg 5 cycles) | at execute: 8 (avg 100 cycles) | "
             "fetch stalled on mispredicts: 81% of cycles");
  REQUIRE(lines.at(4)
          == "test_cpu Mispredict Resolve Latency histogram (cycles): <10: 2 (20%) | 10-24: 0 (0%) | 25-49: 1 (10%) | 50-99: 0 (0%) | 100-199: 6 (60%) | "
             ">=200: 1 (10%)");
}

TEST_CASE("Subtracting core stats subtracts the extended counters")
{
  cpu_stats later{};
  later.total_rob_occupancy = 300;
  later.rob_occupancy_samples = 30;
  later.mispredicts_resolved_at_decode = 5;
  later.mispredicts_resolved_at_execute = 50;
  later.total_mispredict_latency_decode = 25;
  later.total_mispredict_latency_execute = 5000;
  later.mispredict_latency_histogram = {10, 10, 10, 10, 10, 5};

  cpu_stats earlier{};
  earlier.total_rob_occupancy = 100;
  earlier.rob_occupancy_samples = 10;
  earlier.mispredicts_resolved_at_decode = 1;
  earlier.mispredicts_resolved_at_execute = 20;
  earlier.total_mispredict_latency_decode = 5;
  earlier.total_mispredict_latency_execute = 2000;
  earlier.mispredict_latency_histogram = {4, 4, 4, 4, 4, 1};

  auto diff = later - earlier;
  REQUIRE(diff.total_rob_occupancy == 200);
  REQUIRE(diff.rob_occupancy_samples == 20);
  REQUIRE(diff.mispredicts_resolved_at_decode == 4);
  REQUIRE(diff.mispredicts_resolved_at_execute == 30);
  REQUIRE(diff.total_mispredict_latency_decode == 20);
  REQUIRE(diff.total_mispredict_latency_execute == 3000);
  REQUIRE(diff.mispredict_latency_histogram == std::array<uint64_t, 6>{6, 6, 6, 6, 6, 4});
}
