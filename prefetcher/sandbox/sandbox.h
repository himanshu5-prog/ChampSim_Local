#ifndef PREFETCHER_SANDBOX_H
#define PREFETCHER_SANDBOX_H

#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <map>

#include "address.h"
#include "champsim.h"
#include "modules.h"

/*
 * Sandbox Prefetcher (SBP).
 * S. H. Pugsley et al., "Sandbox Prefetching: Safe Run-Time Evaluation of Aggressive Prefetchers", HPCA 2014.
 *
 * Idea: candidate offset prefetchers are tested one at a time in a "sandbox", a Bloom filter that
 * holds the prefetches the candidate *would* have issued. Nothing is fetched while testing. On each
 * L2 access to line A, the sandbox is probed for A, A-O, A-2O and A-3O (a stride-O stream ending at
 * A); each hit adds a point. Then A+O is inserted. After an evaluation period of 256 accesses, the
 * candidate's score is kept, the sandbox is cleared, and the next candidate is tested.
 *
 * 16 candidates are active (initially -8..-1, +1..+8). After each round of all 16, the 4 lowest
 * scorers are swapped for 4 offsets from -16..+16 that were not recently tested.
 *
 * Real prefetches: every evaluated candidate scoring above 256 prefetches A+O (above 512 also
 * A+2O, above 768 also A+3O). Positive offsets go first, smallest magnitude first, until the
 * per-direction degree budget is used; then the same for negative offsets.
 *
 * Scoring checks accuracy only: a sandbox "prefetch" is available at once, so the score does not
 * show whether a real prefetch would arrive in time. Best-Offset was designed to address this.
 */
class sandbox : public champsim::modules::prefetcher
{
public:
  // Parameters: the paper's Table 1 unless noted.
  static constexpr std::size_t FILTER_BITS = 2048;
  static constexpr unsigned FILTER_HASHES = 4; // not given in the paper
  static constexpr int EVAL_PERIOD = 256;      // L2 accesses per candidate
  static constexpr std::size_t ACTIVE = 16;    // candidates tested per round
  static constexpr int CYCLE_OUT = 4;          // lowest scorers replaced after each round
  static constexpr int MAX_OFFSET = 16;        // candidate offsets -16..+16, excluding 0
  static constexpr int STREAM_PROBES = 4;      // probe A, A-O, A-2O, A-3O
  static constexpr std::array<int, 3> CUTOFFS{256, 512, 768}; // score above each: 1, 2, 3 prefetches
  // The paper sets the degree from each core's share of spare memory bandwidth, between 1 and 8
  // prefetches per direction per access, but gives no formula. With one core, this uses the maximum.
  static constexpr int DEGREE = 8;

  using prefetcher::prefetcher;

  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_final_stats();

private:
  struct candidate {
    int offset = 0;
    int score = 0;          // score from its last completed evaluation
    bool evaluated = false; // may issue real prefetches only after one evaluation
  };

  std::bitset<FILTER_BITS> filter{};
  std::array<candidate, ACTIVE> active{};
  std::deque<int> reserve{}; // offsets not active, least recently tested first

  std::size_t current = 0; // candidate under evaluation
  int period_accesses = 0;
  int period_score = 0;

  // statistics
  uint64_t periods = 0;
  uint64_t rounds = 0;
  uint64_t accesses = 0;
  uint64_t issued = 0;
  uint64_t dropped_page = 0;
  uint64_t dropped_queue = 0;
  uint64_t budget_exhausted = 0;
  std::map<int, uint64_t> issued_by_offset{};

  static std::size_t hash(uint64_t line, unsigned i);
  [[nodiscard]] bool filter_test(uint64_t line) const;
  void filter_insert(uint64_t line);
  void evaluate(uint64_t line);
  void end_round();
  void issue(champsim::address addr, uint32_t metadata_in);
};

#endif
