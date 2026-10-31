CXX      ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra

all: tests run

# Unit tests (asserts enabled).
tests: tests.cpp OrderBook.cpp OrderBook.h
	$(CXX) $(CXXFLAGS) -O1 -g tests.cpp OrderBook.cpp -o tests

# Same tests under AddressSanitizer + UBSan.
tests_asan: tests.cpp OrderBook.cpp OrderBook.h
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address,undefined tests.cpp OrderBook.cpp -o tests_asan

# main.cpp: modifyOrder/FOK demo output plus the latency/throughput benchmark.
run: main.cpp OrderBook.cpp OrderBook.h
	$(CXX) $(CXXFLAGS) -O3 main.cpp OrderBook.cpp -o run

test: tests
	./tests

test-asan: tests_asan
	./tests_asan

bench: run
	./run

clean:
	rm -rf tests tests_asan run *.dSYM

.PHONY: all test test-asan bench clean
