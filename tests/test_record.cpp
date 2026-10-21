#include <cassert>
#include <cstdio>
#include <vector>
#include "../src/analyzer.hpp"
using namespace linperf;

static void test_roundtrip() {
    Record r{MAGIC, 7, 555, 0, 123456789ULL};
    uint8_t b[RECORD_SIZE];
    serialize(r, b);
    Record o = deserialize(b);
    assert(o.magic == r.magic && o.ts == 7 && o.cpu_permille == 555 && o.mem_free_kb == 123456789ULL);
}
static void test_little_endian_layout() {
    Record r{MAGIC, 1, 0x0102, 0, 0};
    uint8_t b[RECORD_SIZE];
    serialize(r, b);
    assert(b[0] == 0x01 && b[1] == 0x46 && b[2] == 0x50 && b[3] == 0x4C);  // low byte of magic first
    assert(b[8] == 0x02 && b[9] == 0x01);
}
static void test_byte_order_mismatch_detected() {
    Record r{MAGIC, 1, 10, 0, 1};
    uint8_t b[RECORD_SIZE];
    serialize(r, b);
    assert(get_be(b, 4) != MAGIC);  // reading as big-endian must not look valid
}
static void test_parallel_matches_serial() {
    std::vector<uint8_t> buf(10001 * RECORD_SIZE);
    for (size_t i = 0; i < 10001; ++i) {
        Record r{i % 100 == 0 ? 0xDEADBEEFu : MAGIC, (uint32_t)i, (uint16_t)(i % 1000), 0, 5000 + i};
        serialize(r, buf.data() + i * RECORD_SIZE);
    }
    Stats a = analyze_parallel(buf.data(), 10001, 1);
    for (unsigned t : {2u, 3u, 8u}) {
        Stats b = analyze_parallel(buf.data(), 10001, t);
        assert(a.valid == b.valid && a.invalid == b.invalid && a.cpu_sum == b.cpu_sum &&
               a.cpu_max == b.cpu_max && a.mem_min_kb == b.mem_min_kb);
    }
    assert(a.invalid == 101);
}
int main() {
    test_roundtrip();
    test_little_endian_layout();
    test_byte_order_mismatch_detected();
    test_parallel_matches_serial();
    std::puts("all 4 tests passed");
    return 0;
}
