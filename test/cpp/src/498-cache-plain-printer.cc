#include <catch.hpp>

#include "cache_stats.h"
#include "stats_printer.h"

namespace
{
// The MSHR occupancy line is printed once per cache, after the per-CPU lines, for any cache that has stats.
void append_empty_mshr_line(std::vector<std::string>& expected, const std::string& name)
{
  if (!std::empty(expected)) {
    expected.push_back(name + " MSHR OCCUPANCY: average - | cycles with outstanding misses: -% | MLP (avg outstanding misses when >= 1): -");
  }
}
} // namespace

TEST_CASE("An empty cache stat block prints nothing")
{
  cache_stats given{};
  given.name = "test_cache";

  std::vector<std::string> expected{};

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Hits increment the hit and access counts")
{
  auto num_hits = 255;
  auto [line_index, hit_type, expected_line] = GENERATE(
      as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0"});

  cache_stats given{};
  given.name = "test_cache";
  given.hits.set({hit_type, 0}, num_hits);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:        255 HIT:        255 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};
  expected.at(line_index) = expected_line;

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Misses increment the miss and access counts")
{
  auto num_misses = 255;
  auto [line_index, miss_type, expected_line] = GENERATE(
      as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0"});

  cache_stats given{};
  given.name = "test_cache";
  given.misses.set({miss_type, 0}, num_misses);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};
  expected.at(line_index) = expected_line;

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Returning misses increment the AMAT")
{
  auto num_miss_returned = 128;
  auto num_miss_merged = 127;
  auto miss_return_latency = GENERATE(1, 2, 6);
  auto [line_index, miss_type, expected_line] = GENERATE(
      as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127"},
      // std::tuple{3, access_type::PREFETCH, "test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127"});

  cache_stats given{};
  given.name = "test_cache";
  given.fill.set({miss_type, 0}, num_miss_returned);
  given.miss_merge.set({miss_type, 0}, num_miss_merged);
  given.misses.set({miss_type, 0}, num_miss_merged + num_miss_returned);
  given.total_miss_latency_cycles = miss_return_latency * num_miss_returned;

  std::vector<std::string> expected{
      "cpu0->test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255 MISS_MERGE:        127",
      "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
      "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
      "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
      "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
      "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
      "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
  };
  expected.push_back("cpu0->test_cache AVERAGE MISS LATENCY: " + std::to_string(miss_return_latency) + " cycles");
  expected.at(line_index) = expected_line;

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch requests increase the count")
{
  cache_stats given{};
  given.name = "test_cache";
  given.pf_requested = 1;
  given.fill.set({access_type::PREFETCH, 0}, 1);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          1 ISSUED:          0 USEFUL:          0 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch issues increase the count")
{
  cache_stats given{};
  given.name = "test_cache";
  given.pf_issued = 1;
  given.fill.set({access_type::PREFETCH, 0}, 1);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          1 USEFUL:          0 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useful increases the count")
{
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useful = 1;
  given.fill.set({access_type::PREFETCH, 0}, 1);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          1 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useless increases the count")
{
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useless = 1;
  given.fill.set({access_type::PREFETCH, 0}, 1);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          1",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"};

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Multicore stats are tracked separately")
{
  cache_stats given{};
  given.name = "test_cache";
  auto constexpr cpu0_total_access = 7;
  auto constexpr cpu1_total_access = 11;

  auto [line_index_cpu0, hit_type_cpu0, expected_line_cpu0] = GENERATE(
      as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0"});
  auto [line_index_cpu1, hit_type_cpu1, expected_line_cpu1] = GENERATE(
      as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{9, access_type::LOAD, "cpu1->test_cache LOAD         ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0"},
      std::tuple{10, access_type::RFO, "cpu1->test_cache RFO          ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0"},
      std::tuple{11, access_type::PREFETCH, "cpu1->test_cache PREFETCH     ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0"},
      std::tuple{12, access_type::WRITE, "cpu1->test_cache WRITE        ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0"},
      std::tuple{13, access_type::TRANSLATION, "cpu1->test_cache TRANSLATION  ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0"});
  given.hits.set({hit_type_cpu0, 0}, cpu0_total_access);
  given.hits.set({hit_type_cpu1, 1}, cpu1_total_access);

  std::vector<std::string> expected{"cpu0->test_cache TOTAL        ACCESS:          7 HIT:          7 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
                                    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles",
                                    "cpu1->test_cache TOTAL        ACCESS:         11 HIT:         11 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MISS_MERGE:          0",
                                    "cpu1->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
                                    "cpu1->test_cache AVERAGE MISS LATENCY: - cycles"};
  expected.at(line_index_cpu0) = expected_line_cpu0;
  expected.at(line_index_cpu1) = expected_line_cpu1;

  append_empty_mshr_line(expected, given.name);
  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}
TEST_CASE("MSHR occupancy reports the average, busy fraction, and MLP")
{
  cache_stats given{};
  given.name = "test_cache";
  given.hits.set({access_type::LOAD, 0}, 1);
  given.total_mshr_occupancy = 30;
  given.mshr_samples = 20;
  given.mshr_busy_samples = 10;

  REQUIRE(champsim::plain_printer::format(given).back()
          == "test_cache MSHR OCCUPANCY: average 1.5 | cycles with outstanding misses: 50% | MLP (avg outstanding misses when >= 1): 3");
}

TEST_CASE("Subtracting cache stats subtracts the MSHR counters")
{
  cache_stats later{};
  later.total_mshr_occupancy = 90;
  later.mshr_samples = 60;
  later.mshr_busy_samples = 40;

  cache_stats earlier{};
  earlier.total_mshr_occupancy = 30;
  earlier.mshr_samples = 20;
  earlier.mshr_busy_samples = 15;

  auto diff = later - earlier;
  REQUIRE(diff.total_mshr_occupancy == 60);
  REQUIRE(diff.mshr_samples == 40);
  REQUIRE(diff.mshr_busy_samples == 25);
}
