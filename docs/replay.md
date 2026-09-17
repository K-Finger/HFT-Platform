# Replay

A live socket delivers different data every run, so nothing measured against
it is comparable. `recorder` writes raw frames to a capture file; `replay`
pushes that file through the same parser and ring at full speed, so the same
file produces the same work every time.

## Prerequisites

- A build with `HFT_BUILD_APPS=ON` (the default at top level)
- Disk space: roughly 220 bytes per `bookTicker` frame
- `CAP_SYS_NICE` or root, to run `replay`

## Record and replay

```bash
./build/apps/recorder btcusdt.hftc   # own connection; ctrl-c to stop and flush
sudo ./build/apps/replay btcusdt.hftc
```

`replay` pins itself, takes real-time priority, creates `/dev/shm/hft_ring`,
and reports throughput plus a parse-and-push histogram:

```
replayed 412934 records from btcusdt.hftc in 0.038 s, 10866684 records/s, 411910 dropped
```

Drops are expected — full-speed replay outruns any consumer and the ring is
1024 slots deep. Read the histogram, not the drop count. Replay preserves
each record's original receive timestamp, so downstream sees capture-time
ages, not replay-time.

## The file format

A 64-byte header (magic `HFTC`, version), then records back to back: a
16-byte header (timestamp, payload size), the payload, a NUL terminator,
padding to 8 bytes. Frames are stored raw, not pre-parsed, so replay
exercises the real parser and a parser change shows up in the numbers. No
record count or index — readers walk to EOF, so a file killed mid-write is
valid up to its last whole record.

## Reading a capture in your own code

```cpp
#include "hft/capture/capture_reader.hpp"
#include "hft/feed/book_ticker_parser.hpp"

const hft::capture::CaptureReader reader("btcusdt.hftc");
hft::Message msg{};

for (const hft::capture::CaptureRecord& record : reader)
{
    hft::feed::parse_book_ticker(record.payload, msg);
    msg.timestamp = record.timestamp_ns;
}
```

Records point into the reader's mapping, valid only while the reader is
alive. The constructor validates every magic/version/stride/terminator up
front, so the loop has no bounds checks and a corrupt file fails at open with
the byte offset. Prefer `writer.close()` over the destructor — it can't
throw, so it just reports a failed flush to stderr.

## What replay does not measure

The socket (no TLS, no kernel receive path), the frame copy (live ingestion
copies into a `std::string` first; replay parses straight out of the
mapping), and cross-core transfer (shows up in the consumer's histogram, not
here).

## Benchmarks

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON && cmake --build build
./build/bench/capture_replay_bench
```

`BM_ReplayParse` isolates iteration+parse, `BM_ReplayParseAndPush` adds the
ring, `BM_ReaderOpen` measures the map-and-validate startup cost.

## Troubleshooting

**`is not a capture file: magic 0x...`.** Wrong path, or not written by
`recorder`.

**`truncated: record at offset N needs X bytes, Y remain`.** Lost tail,
usually a full disk. Everything before `N` is intact.

**Replay reports far fewer records than the recorder wrote.** Recorder was
killed before its last flush — stop it with ctrl-c, not SIGKILL.
