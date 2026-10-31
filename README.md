# Matching Engine

A single-threaded limit order book and matching engine in C++20 with price-time priority.

> **Status: work in progress.** The core engine (matching, cancel, modify, order types) works and is tested, but I'm still optimizing it and haven't built everything I want to yet. See [Known limitations](#known-limitations) and [Next steps](#next-steps).

On an Apple M2 Pro (`-O3`), replaying 1M random limit orders:

| Metric | Result |
|---|---|
| p50 latency per order | ~83 ns |
| p99 latency per order | ~330–375 ns |
| Throughput | ~7.5–9M orders/sec |

Numbers vary run to run (throughput ranged ~7.5–9M orders/sec across my runs). Reproduce them with `make bench`.

## Features

- **Order types:** limit, market, IOC (immediate-or-cancel), FOK (fill-or-kill)
- **Cancel** by order id
- **Modify** with exchange-style priority rules: reducing quantity keeps your place in the queue; changing price or increasing quantity cancels and re-enters the order at the back of the queue (and can match immediately if it now crosses)
- **Execution reports** for every fill, including which side was the aggressor
- Trades execute at the **resting order's price**

## Design

- Each side of the book is a `std::map` from price to a FIFO `std::list` of orders, ordered best-price-first (`greater` for bids, `less` for asks). Matching only ever touches the front of the best level.
- An `unordered_map` from order id to `{list iterator, side, price}` means cancel and modify never scan a queue; they jump straight to the order.
- The book's maps and lists use `std::pmr` containers backed by an unsynchronized pool on top of a monotonic buffer, so the hot path mostly avoids the global allocator.
- FOK checks available liquidity at or better than the limit price *before* touching the book, so a killed order never leaves partial changes.

## Build and run

```sh
make test       # unit tests
make test-asan  # same tests under AddressSanitizer + UBSan
make bench   # runs main.cpp: modify/FOK demo output, then the latency and throughput benchmark (-O3)
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
