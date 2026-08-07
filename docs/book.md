# Order book

`hft::book` turns the top-of-book snapshots arriving on the ring into a live limit
order book, and exposes the best bid and offer that strategies read.

The interesting problem is a mismatch: bookTicker sends **state**, a matching
engine consumes **events**. This document covers how that gap is bridged, what the
integer boundary buys, and what the current wiring costs.

## Prerequisites

- The `lib/order-book` submodule, fetched with
  `git submodule update --init --recursive`
- A running `ingestion` or `replay` process filling `/dev/shm/hft_ring`

## Snapshots into orders

Each bookTicker frame is the current best bid and offer. It carries no order ids,
no add, no cancel — just the level as it now stands. A matching engine needs
orders with identities, so `SnapshotBook::apply` treats every frame as a replace:

1. Cancel the two synthetic quotes resting from the previous frame.
2. Insert a good-till-cancel buy at the new bid and sell at the new ask.
3. Record the new top of book for readers that only want the BBO.

The book therefore holds a two-level view a fill simulator or a strategy can match
against, and the class tracks the BBO alongside it. That second part is not
redundancy: the pinned book revision exposes no depth or best-price accessor, only
`size()`, `getTrades()` and `printBook()`, so top of book has to be kept by the
feeder.

Replacing the whole feeder is the intended upgrade path. When a depth or
order-by-order feed lands, write a new class that maintains the same `TopOfBook`
and applies real adds and cancels. Nothing downstream changes.

## Integers at the boundary

`Message` carries doubles because that is what the exchange sends as text. The
book works in integers, and `TickScale` converts once, on entry:

```cpp
constexpr hft::book::TickScale scale{100, 100'000'000};

const std::int64_t  ticks = scale.price_ticks(63501.10);   // 6350110 cents
const std::uint64_t units = scale.quantity_units(1.2);     // 120000000 satoshi
```

Integer prices compare and hash exactly, never accumulate rounding error across a
day of updates, and are what a hardware risk gate or an FPGA path can consume.
Conversion rounds rather than truncates, and rejects a non-finite or negative
value with `std::domain_error` instead of quietly producing a zero.

`kUsdtPairScale` is the Binance USDT-pair scale: prices to the cent, sizes to
eight decimals.

## Reading top of book

```cpp
#include "hft/book/snapshot_book.hpp"

hft::book::SnapshotBook book(hft::book::kUsdtPairScale);
book.apply(msg);

const hft::book::TopOfBook& top = book.top();
const std::int64_t spread        = top.spread_ticks();
const double       fair          = top.microprice_ticks();
```

`microprice_ticks` is the size-weighted mid: the bid is weighted by ask size and
the ask by bid size, so the heavier side pulls the fair price toward the other
side's quote, which is the side likelier to trade next. It throws
`std::domain_error` when neither side shows size, because no fair price exists to
report.

`mid_ticks` is the plain arithmetic mid. `crossed()` flags an ask at or below the
bid, which a real venue does not send and which therefore means a stale or
interleaved update.

## Counters, not silence

`apply` never swallows an anomaly. Each one increments a counter you can read:

- `applied_count` — snapshots accepted.
- `stale_count` — frames whose update id was not newer than the last applied, so
  duplicates and reordered deliveries. bookTicker update ids skip values, so a
  strict "previous plus one" check would report gaps that are not there; only a
  non-increasing id is unambiguously stale.
- `crossed_count` — frames quoting an ask at or below the bid.
- `trade_count` — trades the synthetic quotes generated, which only happens on a
  crossed frame.
- `filled_quote_count` — quotes already gone when the next frame tried to cancel
  them, meaning a crossing frame filled them.

`consumer` prints all five alongside its histograms.

## Cost

`consumer` keeps two histograms rather than one, because the ring hop and the book
update are separate costs with separate fixes:

```
--- ring pop cycles ---
--- book apply cycles ---
```

Expect the book to dominate. Earlier submodule revisions stored orders as
`std::shared_ptr<Order>` in `std::list` price levels and returned trades in a
`std::vector` by value, so a single `apply` performed two cancels, two heap
allocations and two vector returns, and cost hundreds of nanoseconds.

The pinned revision fixes both. Orders come from a slab, so `apply` allocates
nothing after startup, and `addOrder` returns `std::span<const Trade>` over a
buffer the book reuses. `BM_SnapshotBookApply` measures about 26 ns as a result.
The span is only valid until the next `addOrder`, so callers consume it before
touching the book again — `SnapshotBook::rest_quote` sums the fills immediately.

The ring hop is still the cheaper of the two, so the book remains the thing to
look at first if the number matters. The alternative is to keep the LOB out of the
latency path entirely and let strategies read `TopOfBook`, which costs only the
integer conversion — `BM_TickScaleConvert` and `BM_Microprice` measure that path.

Measure before choosing:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/book_bench
```

## Troubleshooting

**`lib/order-book is empty` during configure.** The submodule was never fetched.

```bash
git submodule update --init --recursive
```

**`price -1.000000 is not a finite non-negative number`.** A frame reached the book
with a garbage price, which means the parser accepted something it should not
have. Check the capture around that update id.

**`microprice is undefined with no size on either side`.** Both sides quoted zero
size. Real venues do not send that, so suspect a synthetic or replayed frame.

**`stale_count` climbs on every frame.** Two producers are writing the ring, or a
replay is running against a consumer that already saw newer updates. Only one
process may write the ring.

**`crossed_count` climbs.** Snapshots are arriving out of order, or the producer
and consumer disagree about which instrument the ring carries.

**Book `size()` grows past two.** A cancel is failing without a fill, which means
the resting quote ids and the book have diverged. That is a bug in the feeder, not
a market condition — capture the frame and open an issue.
