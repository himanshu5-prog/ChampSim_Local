#ifndef PREFETCHER_BEST_OFFSET_H
#define PREFETCHER_BEST_OFFSET_H

#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "address.h"
#include "champsim.h"
#include "modules.h"

/*
 * Best-Offset (BO) prefetcher.
 * P. Michaud, "Best-Offset Hardware Prefetching", HPCA 2016. https://inria.hal.science/hal-01254863
 *
 * Idea: prefetch X + D on each L2 demand miss or first hit on a prefetched line X. D is learned by
 * testing candidate offsets d against a Recent Requests (RR) table. RR holds the base address Y - D
 * of prefetches that have *completed* (been filled). If X - d is in RR, a prefetch issued with
 * offset d from that base would have arrived before X was needed, so d scores a point. Scoring
 * on completed prefetches is what makes the chosen offset timely, not just accurate.
 *
 * Learning proceeds in rounds: one candidate offset is tested per trigger access, and a round tests
 * every offset once. A phase ends after ROUND_MAX rounds, or at the end of a round in which some
 * score reached SCORE_MAX. The best-scoring offset becomes D; if its score is <= BAD_SCORE,
 * prefetching is turned off (learning continues).
 */
class best_offset : public champsim::modules::prefetcher
{
public:
  // Parameters: the paper's defaults (Table 2).
  static constexpr std::size_t RR_INDEX_BITS = 8;
  static constexpr std::size_t RR_SIZE = std::size_t{1} << RR_INDEX_BITS; // 256 entries, direct-mapped
  static constexpr unsigned RR_TAG_BITS = 12;
  static constexpr int SCORE_MAX = 31;
  static constexpr int ROUND_MAX = 100;
  static constexpr int BAD_SCORE = 1;
  // The paper's list holds the 52 offsets up to 256 of the form 2^i * 3^j * 5^k. Section 4.2 notes
  // that with 4 KB pages (64 lines) there is no point in offsets above 63, so this keeps the 26
  // offsets up to 63. Note Table 2 still uses all 52 with 4 KB pages, and a shorter list makes
  // each learning round shorter.
  static constexpr int MAX_OFFSET = 63;

  using prefetcher::prefetcher;

  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_final_stats();

private:
  struct rr_entry {
    bool valid = false;
    uint64_t tag = 0;
  };

  std::array<rr_entry, RR_SIZE> rr{};
  std::vector<int> offsets{};
  std::vector<int> scores{};

  std::size_t test_index = 0; // next offset to test
  int round = 0;
  int best_score = 0;         // best score in the current phase
  int phase_best_offset = 0;  // offset holding best_score
  int prefetch_offset = 1;    // D, used for prefetching
  bool prefetch_on = true;

  // statistics
  uint64_t phases = 0;
  uint64_t phases_off = 0;
  uint64_t triggers = 0;
  uint64_t issued = 0;
  uint64_t dropped_page = 0;
  uint64_t dropped_queue = 0;
  std::map<int, uint64_t> chosen_offsets{};

  static std::size_t rr_index(uint64_t line);
  static uint64_t rr_tag(uint64_t line);
  void rr_insert(uint64_t line);
  [[nodiscard]] bool rr_hit(uint64_t line) const;
  void learn(uint64_t line);
};

#endif
