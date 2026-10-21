CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread

all: linperf test_record

linperf: src/main.cpp src/record.hpp src/analyzer.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp -o linperf

test_record: tests/test_record.cpp src/record.hpp src/analyzer.hpp
	$(CXX) $(CXXFLAGS) -UNDEBUG tests/test_record.cpp -o test_record

test: test_record
	./test_record

clean:
	rm -f linperf test_record *.bin
