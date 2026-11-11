#include "best_offset.h"

#include <algorithm>
#include <fmt/core.h>

#include "cache.h"

void best_offset::prefetcher_initialize()
{
  // Offsets 1..MAX_OFFSET whose only prime factors are 2, 3 and 5
  for (int d = 1; d <= MAX_OFFSET; ++d) {
    int r = d;
    for (int f : {2, 3, 5}) {
      while (r % f == 0) {
        r /= f;
      }
    }
    if (r == 1) {
      offsets.push_back(d);
    }
  }
  scores.assign(std::size(offsets), 0);
}

// Direct-mapped: index = low 8 line-address bits XOR the next 8; tag = 12 bits above the low 8.
std::size_t best_offset::rr_index(uint64_t line) { return static_cast<std::size_t>((line ^ (line >> RR_INDEX_BITS)) & (RR_SIZE - 1)); }

uint64_t best_offset::rr_tag(uint64_t line) { return (line >> RR_INDEX_BITS) & ((uint64_t{1} << RR_TAG_BITS) - 1); }

void best_offset::rr_insert(uint64_t line) { rr.at(rr_index(line)) = {true, rr_tag(line)}; }

bool best_offset::rr_hit(uint64_t line) const
{
  const auto& entry = rr.at(rr_index(line));
  return entry.valid && entry.tag == rr_tag(line);
}

void best_offset::learn(uint64_t line)
{
  const int d = offsets.at(test_index);
  if (line >= static_cast<uint64_t>(d) && rr_hit(line - static_cast<uint64_t>(d))) {
    auto& score = scores.at(test_index);
    ++score;
    if (score > best_score) {
      best_score = score;
      phase_best_offset = d;
    }
  }

  if (++test_index < std::size(offsets)) {
    return;
  }

  // End of a round
  test_index = 0;
  ++round;
  if (best_score >= SCORE_MAX || round >= ROUND_MAX) {
    // End of a learning phase
    ++phases;
    prefetch_on = best_score > BAD_SCORE;
    if (prefetch_on) {
      prefetch_offset = phase_best_offset;
      ++chosen_offsets[prefetch_offset];
    } else {
      ++phases_off;
    }
    round = 0;
    best_score = 0;
    phase_best_offset = 0;
    std::fill(std::begin(scores), std::end(scores), 0);
  }
}

uint32_t best_offset::prefetcher_cache_operate(champsim::address addr, champsim::address /*ip*/, uint8_t cache_hit, bool useful_prefetch,
                                               access_type type, uint32_t metadata_in)
{
  // Triggers: read misses, and the first read hit on a prefetched line. Reads include prefetches
  // from upper-level caches (paper section 5.5); ChampSim never passes this cache's own prefetches here.
  if (type != access_type::LOAD && type != access_type::RFO && type != access_type::PREFETCH) {
    return metadata_in;
  }
  if (cache_hit && !useful_prefetch) {
    return metadata_in;
  }

  ++triggers;
  const champsim::block_number block{addr};
  learn(block.to<uint64_t>());

  if (prefetch_on) {
    const auto pf_block = block + prefetch_offset;
    const champsim::address pf_addr{pf_block};
    if (champsim::page_number{pf_addr} != champsim::page_number{addr}) {
      ++dropped_page;
    } else if (prefetch_line(pf_addr, true, metadata_in)) {
      ++issued;
    } else {
      ++dropped_queue;
    }
  }

  return metadata_in;
}

uint32_t best_offset::prefetcher_cache_fill(champsim::address addr, long /*set*/, long /*way*/, uint8_t prefetch, champsim::address /*evicted_addr*/,
                                            uint32_t metadata_in)
{
  const uint64_t line = champsim::block_number{addr}.to<uint64_t>();
  if (prefetch_on) {
    // A prefetch for Y completed: record its base address Y - D. If Y - D falls in another page,
    // the real base is unknown and RR is not written (paper footnote 2).
    if (prefetch && line >= static_cast<uint64_t>(prefetch_offset)) {
      const uint64_t base = line - static_cast<uint64_t>(prefetch_offset);
      if (champsim::page_number{champsim::address{champsim::block_number{base}}} == champsim::page_number{addr}) {
        rr_insert(base);
      }
    }
  } else {
    // Prefetching is off: record demand fills as if D = 0, so learning can find an offset again
    rr_insert(line);
  }
  return metadata_in;
}

void best_offset::prefetcher_final_stats()
{
  fmt::print("{} Best-Offset: phases {} (prefetch off after {}) | triggers {} | prefetches issued {} | dropped at page boundary {} | dropped, queue full {} | "
             "current offset {} ({})\n",
             intern_->NAME, phases, phases_off, triggers, issued, dropped_page, dropped_queue, prefetch_offset, prefetch_on ? "on" : "off");
  std::string chosen = fmt::format("{} Best-Offset chosen offsets (phases):", intern_->NAME);
  for (auto [offset, count] : chosen_offsets) {
    chosen += fmt::format(" {}:{}", offset, count);
  }
  fmt::print("{}\n", chosen);
}
