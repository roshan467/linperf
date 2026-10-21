#!/usr/bin/env bash
# Build, test, generate data, benchmark, and sample the live system.
set -euo pipefail
cd "$(dirname "$0")/.."
make all
make test
./linperf endian
./linperf gen sample.bin 5000000
./linperf bench sample.bin
./linperf monitor live.bin 5 500
./linperf analyze live.bin 2
