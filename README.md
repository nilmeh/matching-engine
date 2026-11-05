# Matching Engine

A single-threaded limit order book and matching engine in C++20 with price-time priority.

> **Status: work in progress.** The core engine (matching, cancel, modify, order types) works and is tested, but I'm still optimizing it and haven't built everything I want to yet. See [Known limitations](#known-limitations) and [Next steps](#next-steps).

Benchmark: replays 1M random limit orders (prices 9900–10100, quantity 1–100, random side, fixed seed, no cancels) on an Apple M2 Pro, `-O3`:

| Metric | Result |
|---|---|
| p50 latency per order | ~83 ns (timer resolution on this machine is ~42 ns, so this is only accurate to about ±1 tick) |
| p99 latency per order | ~330–375 ns |
| Throughput | ~7.3–8.8M orders/sec |

Numbers vary run to run. Reproduce them with `make bench`. This workload has no cancels or modifies, so it is not representative of a real feed.

## Features

- **Order types:** limit, market, IOC (immediate-or-cancel), FOK (fill-or-kill)
- **Cancel** by order id
- **Modify** with exchange-style priority rules: reducing quantity keeps your place in the queue; changing price or increasing quantity cancels and re-enters the order at the back of the queue (and can match immediately if it now crosses)
- **Execution reports** for every fill, including which side was the aggressor
- Trades execute at the **resting order's price**

## Design

- Each side of the book is a `std::map` from price to a FIFO `std::list` of orders, ordered best-price-first (`greater` for bids, `less` for asks). Matching only ever touches the front of the best level.
- An `unordered_map` from order id to `{list iterator, side, price}` means cancel and modify never scan a queue; they jump straight to the order.
- The book's maps and lists use `std::pmr` containers backed by an unsynchronized pool on top of a monotonic buffer, which cuts allocator calls for book nodes. The id lookup `unordered_map` and the returned `std::vector<ExecutionReport>` still use the global allocator (about 1.4 heap allocations per order in my measurements).
- FOK checks available liquidity at or better than the limit price *before* touching the book, so a killed order never leaves partial changes.

## Build and run

```sh
make test       # unit tests
make test-asan  # same tests under AddressSanitizer + UBSan
make bench      # runs main.cpp: modify/FOK demo output, then the latency and throughput benchmark (-O3)
```

Requires a C++20 compiler (tested with Apple clang on macOS).

## Tests

`tests.cpp` has 17 assert-based tests covering price-time priority, multi-level sweeps, partial fills, cancel, market/IOC/FOK semantics, and every modify case. One is a regression test for a bug where a partially filled limit order rested with its original quantity instead of the remaining quantity.

## Known limitations

- Single-threaded; no locking or lock-free queues
- No self-trade prevention
- Orders with a duplicate id of a resting order, or with zero quantity, are silently ignored (no reject report)
- Prices are integer ticks; there is no tick-size or price-band validation
- `std::map` and `std::list` are cache-unfriendly
- `matchAgainstBook` has duplicated buy and sell branches
- The demo output in `main.cpp` is printed, not asserted; the real checks are in `tests.cpp`

## Next steps

- Try a flat array of price levels indexed by tick instead of `std::map`, and measure the difference
- Deduplicate the buy/sell matching code
- Reject reports for invalid orders instead of silently ignoring them
- Self-trade prevention
