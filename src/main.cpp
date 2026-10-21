// linperf: Linux performance sampler and multi-threaded binary log analyzer.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include "analyzer.hpp"
#include "record.hpp"

using namespace linperf;
using Clock = std::chrono::steady_clock;

static bool read_cpu_times(uint64_t& busy, uint64_t& total) {
    std::ifstream f("/proc/stat");
    std::string label;
    if (!(f >> label) || label != "cpu") return false;
    uint64_t v[8] = {0};
    for (int i = 0; i < 8; ++i) f >> v[i];  // user nice system idle iowait irq softirq steal
    uint64_t idle = v[3] + v[4];
    total = 0;
    for (int i = 0; i < 8; ++i) total += v[i];
    busy = total - idle;
    return true;
}

static uint64_t read_mem_available_kb() {
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MemAvailable:", 0) == 0) return std::strtoull(line.c_str() + 13, nullptr, 10);
    }
    return 0;
}

static std::vector<uint8_t> load_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << "\n"; std::exit(1); }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static int cmd_monitor(const std::string& path, int seconds, int interval_ms) {
    std::ofstream out(path, std::ios::binary);
    uint64_t b0, t0;
    if (!read_cpu_times(b0, t0)) { std::cerr << "/proc/stat not readable (Linux required)\n"; return 1; }
    int samples = seconds * 1000 / interval_ms;
    for (int i = 0; i < samples; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        uint64_t b1, t1;
        read_cpu_times(b1, t1);
        double busy = (t1 > t0) ? double(b1 - b0) / double(t1 - t0) : 0.0;
        Record r{MAGIC, static_cast<uint32_t>(i * interval_ms / 1000),
                 static_cast<uint16_t>(busy * 1000), 0, read_mem_available_kb()};
        uint8_t buf[RECORD_SIZE];
        serialize(r, buf);
        out.write(reinterpret_cast<char*>(buf), RECORD_SIZE);
        b0 = b1; t0 = t1;
    }
    std::printf("wrote %d records to %s\n", samples, path.c_str());
    return 0;
}

static int cmd_gen(const std::string& path, size_t count) {
    std::ofstream out(path, std::ios::binary);
    std::mt19937_64 rng(42);
    std::vector<uint8_t> buf(count * RECORD_SIZE);
    for (size_t i = 0; i < count; ++i) {
        Record r{MAGIC, static_cast<uint32_t>(i), static_cast<uint16_t>(rng() % 1001), 0, 1000000 + rng() % 8000000};
        serialize(r, buf.data() + i * RECORD_SIZE);
    }
    out.write(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    std::printf("generated %zu records (%zu bytes) in %s\n", count, buf.size(), path.c_str());
    return 0;
}

static void print_stats(const Stats& s) {
    std::printf("valid=%llu invalid=%llu cpu_avg=%.1f%% cpu_max=%.1f%% mem_min=%llu kB\n",
                (unsigned long long)s.valid, (unsigned long long)s.invalid, s.cpu_avg_percent(),
                s.cpu_max / 10.0, (unsigned long long)s.mem_min_kb);
}

static double time_ms(const std::vector<uint8_t>& data, unsigned threads, Stats& out) {
    auto t0 = Clock::now();
    out = analyze_parallel(data.data(), data.size() / RECORD_SIZE, threads);
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

static int cmd_analyze(const std::string& path, unsigned threads) {
    auto data = load_file(path);
    Stats s;
    double ms = time_ms(data, threads, s);
    print_stats(s);
    std::printf("threads=%u time=%.2f ms\n", threads, ms);
    return 0;
}

static int cmd_bench(const std::string& path) {
    auto data = load_file(path);
    unsigned hw = std::max(2u, std::thread::hardware_concurrency());
    Stats s1, sn;
    double t1 = time_ms(data, 1, s1), tn = time_ms(data, hw, sn);
    std::printf("1 thread : %.2f ms\n%u threads: %.2f ms (speedup %.2fx)\n", t1, hw, tn, tn > 0 ? t1 / tn : 0.0);
    std::printf("results match: %s\n", (s1.valid == sn.valid && s1.cpu_sum == sn.cpu_sum) ? "yes" : "NO");
    return 0;
}

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "endian") {
        std::printf("host byte order: %s-endian; log format: little-endian\n", host_is_little_endian() ? "little" : "big");
        return 0;
    }
    if (cmd == "monitor" && argc >= 5) return cmd_monitor(argv[2], std::atoi(argv[3]), std::atoi(argv[4]));
    if (cmd == "gen" && argc >= 4) return cmd_gen(argv[2], std::strtoull(argv[3], nullptr, 10));
    if (cmd == "analyze" && argc >= 3) return cmd_analyze(argv[2], argc > 3 ? std::atoi(argv[3]) : 4);
    if (cmd == "bench" && argc >= 3) return cmd_bench(argv[2]);
    std::puts("usage:\n  linperf endian\n  linperf monitor <file> <seconds> <interval_ms>\n"
              "  linperf gen <file> <count>\n  linperf analyze <file> [threads]\n  linperf bench <file>");
    return 1;
}
