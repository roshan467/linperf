// Multi-threaded analysis of a buffer of serialized records.
#pragma once
#include <algorithm>
#include <limits>
#include <thread>
#include <vector>
#include "record.hpp"

namespace linperf {

struct Stats {
    uint64_t valid = 0;
    uint64_t invalid = 0;
    uint64_t cpu_sum = 0;
    uint16_t cpu_max = 0;
    uint64_t mem_min_kb = std::numeric_limits<uint64_t>::max();

    void merge(const Stats& o) {
        valid += o.valid;
        invalid += o.invalid;
        cpu_sum += o.cpu_sum;
        cpu_max = std::max(cpu_max, o.cpu_max);
        mem_min_kb = std::min(mem_min_kb, o.mem_min_kb);
    }
    double cpu_avg_percent() const { return valid ? (cpu_sum / 10.0) / valid : 0.0; }
};

inline Stats analyze_range(const uint8_t* buf, size_t first, size_t last) {
    Stats s;
    for (size_t i = first; i < last; ++i) {
        Record r = deserialize(buf + i * RECORD_SIZE);
        if (r.magic != MAGIC) { ++s.invalid; continue; }
        ++s.valid;
        s.cpu_sum += r.cpu_permille;
        s.cpu_max = std::max(s.cpu_max, r.cpu_permille);
        s.mem_min_kb = std::min(s.mem_min_kb, r.mem_free_kb);
    }
    return s;
}

// Splits records evenly across threads; each thread writes only its own Stats slot (no locks needed).
inline Stats analyze_parallel(const uint8_t* buf, size_t n_records, unsigned threads) {
    if (threads < 2 || n_records < threads) return analyze_range(buf, 0, n_records);
    std::vector<Stats> partial(threads);
    std::vector<std::thread> pool;
    size_t chunk = n_records / threads;
    for (unsigned t = 0; t < threads; ++t) {
        size_t a = t * chunk;
        size_t b = (t + 1 == threads) ? n_records : a + chunk;
        pool.emplace_back([&, t, a, b] { partial[t] = analyze_range(buf, a, b); });
    }
    for (auto& th : pool) th.join();
    Stats total;
    for (const auto& p : partial) total.merge(p);
    return total;
}

}  // namespace linperf
