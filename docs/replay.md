# Replay

Replay makes latency numbers reproducible. A live socket delivers different data
every run, so nothing measured against it can be compared. `recorder` writes raw
frames to a capture file, `replay` pushes that file through the same parser and
the same ring at full speed, and the same file produces the same work every time.
A change in the histogram is then a change in your code.

## Prerequisites

- A build with `HFT_BUILD_APPS=ON`, the default at top level
- Disk space for the capture: roughly 220 bytes per `bookTicker` frame
- `CAP_SYS_NICE` or root, to run `replay`

## Record a capture

1. Start the recorder with an output path. It opens its own websocket
   connection, so it never disturbs a running `ingestion` process.

   ```bash
   ./build/apps/recorder btcusdt.hftc
   ```

2. Leave it running through the market conditions you care about. A minute of
   `bookTicker` is enough for a first measurement; a busy hour gives a tail worth
   quoting.

3. Stop it with ctrl-c. It flushes, closes and prints the frame count.

The recorder is deliberately ordinary: unpinned, no real-time priority, buffered
writes. Recording is data collection, not trading, and disk writes have no
business inside a pinned thread.

## Replay it

```bash
sudo ./build/apps/replay btcusdt.hftc
```

`replay` pins itself, takes real-time priority, creates `/dev/shm/hft_ring`, then
walks the capture and reports throughput plus a histogram of parse-and-push cycles:

```
replayed 412934 records from btcusdt.hftc in 0.038 s, 10866684 records/s, 411910 dropped
```

Drops are expected. A full-speed replay outruns any consumer, and the ring is
1024 slots deep. The point of this run is the per-record cost, not delivery. Start
a consumer if you want to watch it work, but read the histogram, not the drop
count.

Replay preserves each record's original receive timestamp in
`Message::timestamp`, so anything downstream that computes message age sees
capture-time values rather than replay-time ones.

## The file format

A 64-byte file header holds the magic `HFTC` and a format version. Records follow
back to back, each one a 16-byte header (receive timestamp, payload size), then
the payload, then a NUL terminator, then padding to the next 8-byte boundary.

Three decisions are worth knowing:

**Raw frames, not parsed messages.** The capture stores exactly what the exchange
sent. Replay therefore exercises the real parser, so a parser change shows up in
the measurement. Storing `Message` records instead would benchmark a memcpy.

**No record count and no index.** Readers walk to EOF. A recorder killed with
SIGKILL leaves a file that is valid up to its last whole record, with no
finalisation step and no repair tool.

**Payloads are NUL terminated and headers stay 8-byte aligned.** The terminator
lets `parse_book_ticker` take a `const char*` pointing straight into the mapping,
with no copy and no bounded scan. The alignment lets the reader pointer-cast each
header instead of reassembling it byte by byte.

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

Records point into the reader's mapping, so both the record and the payload it
carries are only valid while that reader is alive.

The constructor maps the file and walks it once, checking every magic, version,
stride and terminator. That means the loop above runs with no bounds checks and no
error handling, and a corrupt file fails at open with the byte offset that broke.

Writing is the same shape:

```cpp
#include "hft/capture/capture_writer.hpp"

hft::capture::CaptureWriter writer("btcusdt.hftc");
writer.append(hft::time::now_ns(), frame);
writer.close();
```

Call `close()` rather than leaning on the destructor. The destructor cannot throw,
so it reports a failed flush to stderr instead.

## What replay does not measure

**The socket.** Everything upstream of the parser is gone: no TLS, no kernel
receive path, no interrupt. Replay measures parse plus publish, which is the part
you control.

**The frame copy.** Live ingestion copies the websocket payload into a
`std::string` before parsing. Replay parses straight out of the mapping, so it
reads a few nanoseconds faster than the live path on the same data.

**Cross-core transfer.** `replay` measures its own push. The cost of the cache
line moving to a consumer core shows up in the consumer's histogram, not here.

## Benchmarks

`capture_replay_bench` runs the same loop under Google Benchmark against a
synthetic 100,000 record capture:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DHFT_BUILD_BENCH=ON
cmake --build build
./build/bench/capture_replay_bench
```

`BM_ReplayParse` isolates iteration plus parse. `BM_ReplayParseAndPush` adds the
ring. `BM_ReaderOpen` measures the map-and-validate startup cost, which scales
with file size and is charged once per run.

## Troubleshooting

**`is not a capture file: magic 0x...`.** The path points at something else. Check
the file and confirm it was written by `recorder`.

**`uses capture format version N, this build reads M`.** The file predates a format
change. Re-record it, or check out the build that wrote it.

**`truncated: record at offset N needs X bytes, Y remain`.** The file lost its tail,
usually a full disk or a copy that stopped early. Everything before that offset is
intact, so re-record or trim the file at `N`.

**`record at offset N is not NUL terminated`.** The file was modified after
recording. Re-record it.

**`capture payload of N bytes exceeds the 1048576 byte write buffer`.** A single
frame is larger than `CaptureWriter::kBufferBytes`. Raise that constant if the
venue really sends frames that big.

**`capture writer failed to close ...` on stderr.** The final flush failed, almost
always a full disk. The records already flushed are still readable.

**Replay reports far fewer records than the recorder wrote.** The recorder was
killed before its last flush. Whole records survive; the buffered tail does not.
Stop it with ctrl-c rather than SIGKILL.

**Replay throughput swings between runs.** Frequency scaling or a shared core. Set
the `performance` governor and pin to an isolated core, as in
[user_guide.md](user_guide.md).
