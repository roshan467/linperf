# LinPerf – Linux Performance Monitor and Endian-Aware Binary Log Analyzer (C++17)

A command-line tool that samples CPU and memory from `/proc`, writes fixed-size binary records in an explicit
little-endian format, and analyses large logs with multiple threads. Built to show systems programming:
C++, Linux internals, multi-threading, byte order, testing and automation.

## Build and run (Linux)
```bash
./scripts/run_demo.sh        # build, test, generate data, benchmark, sample the live system
make test                    # 4 unit tests (round-trip, byte layout, byte-order mismatch, parallel == serial)
./linperf endian | monitor <file> <sec> <ms> | gen <file> <n> | analyze <file> [threads] | bench <file>
```

## Design notes
- Records are serialized byte by byte, so the file reads identically on little-endian and big-endian CPUs.
- The analyzer splits records across `std::thread`s; each thread writes only its own result slot, so no locks are needed.
- `/proc/stat` and `/proc/meminfo` are parsed directly for CPU busy time and available memory.
- Measured (single-core cloud VM): 5,000,000 records (100 MB) analysed in about 43-55 ms single-threaded, results
  identical across 1, 2, 3 and 8 threads (unit-tested). A single core shows no multi-thread speed-up; run
  `./linperf bench` on a multi-core machine and record your own numbers here.

## Ideas to extend
Profile with `perf`, add a `--follow` mode, or compare against `mmap`-based reading.
