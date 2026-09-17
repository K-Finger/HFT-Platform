# Order book

`hft::book` turns top-of-book snapshots from the ring into a live limit order
book and exposes the BBO. The interesting problem: bookTicker sends
**state**, a matching engine consumes **events**.

## Prerequisites

- The `lib/order-book` submodule: `git submodule update --init --recursive`
- A running `ingestion` or `replay` process filling `/dev/shm/hft_ring`

## Snapshots into orders

Each frame is just the current best bid/offer — no order ids, no add, no
cancel. `SnapshotBook::apply` treats every frame as a replace: cancel the
previous frame's two synthetic quotes, insert a new bid and ask, record the
new top of book.

The pinned `lib/order-book` revision exposes no depth/best-price accessor
(only `size()`, `getTrades()`, `printBook()`), so top of book is tracked
separately by the feeder. The intended upgrade path is a new feeder class
that maintains the same `TopOfBook` from real adds/cancels off a depth feed —
nothing downstream would change.

## Integers at the boundary

`TickScale` converts the feed's doubles into ticks once, on entry:

```cpp
constexpr hft::book::TickScale scale{100, 100'000'000};

const std::int64_t  ticks = scale.price_ticks(63501.10);   // 6350110 cents
const std::uint64_t units = scale.quantity_units(1.2);     // 120000000 satoshi
```

Integers compare exactly and never accumulate rounding error. Conversion
rounds and rejects non-finite/negative values with `std::domain_error`.
`kUsdtPairScale` is the Binance USDT-pair scale: cents, eight-decimal sizes.

## Reading top of book

```cpp
#include "hft/book/snapshot_book.hpp"

hft::book::SnapshotBook book(hft::book::kUsdtPairScale);
book.apply(msg);

const hft::book::TopOfBook& top = book.top();
const std::int64_t spread        = top.spread_ticks();
const double       fair          = top.microprice_ticks();
```

`microprice_ticks` is the size-weighted mid (throws `std::domain_error` if
neither side shows size). `mid_ticks` is the plain mid. `crossed()` flags an
ask at or below the bid — a stale or interleaved update.

## Counters, not silence

`apply` increments a counter for every anomaly instead of swallowing it:
`applied_count`, `stale_count` (non-increasing update id — ids can skip, so
only non-increasing counts as stale), `crossed_count`, `trade_count`
(synthetic fills from a crossed frame), `filled_quote_count` (quote already
gone when the next frame tried to cancel it). `consumer` prints all five.

## Cost

`consumer` keeps separate histograms for the ring hop and the book update.
Expect the book to dominate: earlier revisions used `shared_ptr<Order>` in
`std::list` and returned trades by value, costing hundreds of ns per apply.
The pinned revision allocates nothing (slab-backed orders) and returns
`std::span<const Trade>` over a reused buffer — `BM_SnapshotBookApply`
measures ~26ns. The span is only valid until the next `addOrder`, so consume
it immediately (`SnapshotBook::rest_quote` does).

The ring hop is still cheaper, so optimize the book first if it matters.
Alternative: skip the LOB and let strategies read `TopOfBook` directly — just
the integer conversion (`BM_TickScaleConvert`, `BM_Microprice`).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON && cmake --build build
./build/bench/book_bench
```

## Troubleshooting

**`lib/order-book is empty` during configure.** Submodule not fetched:
`git submodule update --init --recursive`.

**`stale_count` climbs on every frame.** Two producers writing the ring, or a
replay racing a consumer that already saw newer updates.

**Book `size()` grows past two.** A cancel failed without a fill — a feeder
bug, not a market condition. Capture the frame and open an issue.
