#include "sandbox.h"

#include <algorithm>
#include <cstdlib>
#include <fmt/core.h>
#include <numeric>
#include <vector>

#include "cache.h"

void sandbox::prefetcher_initialize()
{
  // Active: -8..-1 and +1..+8. Reserve: the rest of -16..+16, nearest to zero first.
  std::size_t i = 0;
  for (int d = 1; d <= MAX_OFFSET; ++d) {
    for (int o : {d, -d}) {
      if (d <= static_cast<int>(ACTIVE / 2)) {
        active.at(i++).offset = o;
      } else {
        reserve.push_back(o);
      }
    }
  }
}

std::size_t sandbox::hash(uint64_t line, unsigned i)
{
  // Multiplicative hashing with a different odd constant per hash function
  static constexpr std::array<uint64_t, 4> K{0x9E3779B97F4A7C15ULL, 0xC2B2AE3D27D4EB4FULL, 0x165667B19E3779F9ULL, 0xD6E8FEB86659FD93ULL};
  return static_cast<std::size_t>((line * K.at(i)) >> 53) % FILTER_BITS; // top 11 bits
}

bool sandbox::filter_test(uint64_t line) const
{
  for (unsigned i = 0; i < FILTER_HASHES; ++i) {
    if (!filter.test(hash(line, i))) {
      return false;
    }
  }
  return true;
}

void sandbox::filter_insert(uint64_t line)
{
  for (unsigned i = 0; i < FILTER_HASHES; ++i) {
    filter.set(hash(line, i));
  }
}

void sandbox::evaluate(uint64_t line)
{
  const int o = active.at(current).offset;

  // Probe A, A-O, A-2O, A-3O: each found means a stride-O stream would have covered it
  for (int k = 0; k < STREAM_PROBES; ++k) {
    const auto probe = static_cast<int64_t>(line) - static_cast<int64_t>(k) * o;
    if (probe >= 0 && filter_test(static_cast<uint64_t>(probe))) {
      ++period_score;
    }
  }
  // The candidate's simulated prefetch
  const auto target = static_cast<int64_t>(line) + o;
  if (target >= 0) {
    filter_insert(static_cast<uint64_t>(target));
  }

  if (++period_accesses < EVAL_PERIOD) {
    return;
  }

  // End of this candidate's evaluation period
  ++periods;
  auto& c = active.at(current);
  c.score = period_score;
  c.evaluated = true;
  filter.reset();
  period_accesses = 0;
  period_score = 0;

  if (++current == ACTIVE) {
    current = 0;
    end_round();
  }
}

void sandbox::end_round()
{
  ++rounds;
  // Swap the CYCLE_OUT lowest scorers for the reserve offsets tested least recently
  std::vector<std::size_t> order(ACTIVE);
  std::iota(std::begin(order), std::end(order), std::size_t{0});
  std::stable_sort(std::begin(order), std::end(order), [this](std::size_t a, std::size_t b) { return active.at(a).score < active.at(b).score; });
  for (int k = 0; k < CYCLE_OUT; ++k) {
    auto& c = active.at(order.at(static_cast<std::size_t>(k)));
    reserve.push_back(c.offset);
    c = candidate{reserve.front(), 0, false};
    reserve.pop_front();
  }
}

void sandbox::issue(champsim::address addr, uint32_t metadata_in)
{
  // Evaluated candidates above the cutoff, positive offsets first, smallest magnitude first
  std::vector<const candidate*> ready;
  for (const auto& c : active) {
    if (c.evaluated && c.score > CUTOFFS.front()) {
      ready.push_back(&c);
    }
  }
  std::sort(std::begin(ready), std::end(ready), [](const candidate* a, const candidate* b) {
    if ((a->offset > 0) != (b->offset > 0)) {
      return a->offset > 0;
    }
    return std::abs(a->offset) < std::abs(b->offset);
  });

  const champsim::block_number block{addr};
  int budget_pos = DEGREE;
  int budget_neg = DEGREE;
  for (const candidate* c : ready) {
    int& budget = c->offset > 0 ? budget_pos : budget_neg;
    const auto depth = std::count_if(std::begin(CUTOFFS), std::end(CUTOFFS), [c](int cut) { return c->score > cut; });
    for (long k = 1; k <= depth; ++k) {
      if (budget == 0) {
        ++budget_exhausted;
        break;
      }
      --budget;
      const auto pf_block = block + (k * c->offset);
      const champsim::address pf_addr{pf_block};
      if (champsim::page_number{pf_addr} != champsim::page_number{addr}) {
        ++dropped_page;
        break; // further along this stream is past the page too
      }
      if (prefetch_line(pf_addr, true, metadata_in)) {
        ++issued;
        ++issued_by_offset[c->offset];
      } else {
        ++dropped_queue;
      }
    }
  }
}

uint32_t sandbox::prefetcher_cache_operate(champsim::address addr, champsim::address /*ip*/, uint8_t /*cache_hit*/, bool /*useful_prefetch*/,
                                           access_type type, uint32_t metadata_in)
{
  // SBP acts on every L2 read access, hit or miss
  if (type != access_type::LOAD && type != access_type::RFO && type != access_type::PREFETCH) {
    return metadata_in;
  }
  ++accesses;
  evaluate(champsim::block_number{addr}.to<uint64_t>());
  issue(addr, metadata_in);
  return metadata_in;
}

uint32_t sandbox::prefetcher_cache_fill(champsim::address /*addr*/, long /*set*/, long /*way*/, uint8_t /*prefetch*/, champsim::address /*evicted_addr*/,
                                        uint32_t metadata_in)
{
  return metadata_in;
}

void sandbox::prefetcher_final_stats()
{
  fmt::print("{} Sandbox: accesses {} | evaluation periods {} | rounds {} | prefetches issued {} | dropped at page boundary {} | dropped, queue full {} | "
             "degree budget used up {}\n",
             intern_->NAME, accesses, periods, rounds, issued, dropped_page, dropped_queue, budget_exhausted);
  std::string scores = fmt::format("{} Sandbox final candidate scores (offset:score):", intern_->NAME);
  for (const auto& c : active) {
    scores += fmt::format(" {}:{}", c.offset, c.evaluated ? std::to_string(c.score) : std::string{"-"});
  }
  fmt::print("{}\n", scores);
  std::string by_offset = fmt::format("{} Sandbox prefetches issued by offset:", intern_->NAME);
  for (auto [offset, count] : issued_by_offset) {
    by_offset += fmt::format(" {}:{}", offset, count);
  }
  fmt::print("{}\n", by_offset);
}
