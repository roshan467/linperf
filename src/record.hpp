// Fixed-size binary log record with explicit, endian-independent (de)serialization.
#pragma once
#include <cstddef>
#include <cstdint>

namespace linperf {

constexpr uint32_t MAGIC = 0x4C504601;   // "LPF" + version 1
constexpr size_t RECORD_SIZE = 20;       // 4 + 4 + 2 + 2 + 8 bytes

struct Record {
    uint32_t magic;
    uint32_t ts;            // seconds since start
    uint16_t cpu_permille;  // CPU busy time in tenths of a percent (0-1000)
    uint16_t reserved;
    uint64_t mem_free_kb;   // MemAvailable from /proc/meminfo
};

inline bool host_is_little_endian() {
    const uint16_t x = 1;
    return *reinterpret_cast<const uint8_t*>(&x) == 1;
}

// Stored on disk as little-endian regardless of host byte order.
inline void put_le(uint8_t* p, uint64_t v, int n) {
    for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
inline uint64_t get_le(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v |= static_cast<uint64_t>(p[i]) << (8 * i);
    return v;
}
// Big-endian reader, used to show what a byte-order mismatch looks like.
inline uint64_t get_be(const uint8_t* p, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}

inline void serialize(const Record& r, uint8_t* out) {
    put_le(out, r.magic, 4);
    put_le(out + 4, r.ts, 4);
    put_le(out + 8, r.cpu_permille, 2);
    put_le(out + 10, r.reserved, 2);
    put_le(out + 12, r.mem_free_kb, 8);
}

inline Record deserialize(const uint8_t* in) {
    Record r;
    r.magic = static_cast<uint32_t>(get_le(in, 4));
    r.ts = static_cast<uint32_t>(get_le(in + 4, 4));
    r.cpu_permille = static_cast<uint16_t>(get_le(in + 8, 2));
    r.reserved = static_cast<uint16_t>(get_le(in + 10, 2));
    r.mem_free_kb = get_le(in + 12, 8);
    return r;
}

}  // namespace linperf
