#include "hft/capture/capture_reader.hpp"
#include "hft/capture/capture_writer.hpp"
#include "hft/feed/book_ticker_parser.hpp"
#include "hft/ipc/spsc_ring.hpp"

#include "fixtures/frames.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace
{

constexpr std::size_t kRecords = 100'000;

/* Written once per process, then mapped by every iteration below. */
const std::string& capture_path()
{
    static const std::string path = [] {
        const std::string file =
            (std::filesystem::temp_directory_path() / "hft_capture_bench.hftc").string();

        hft::capture::CaptureWriter writer(file);
        for (std::size_t i = 0; i < kRecords; i++)
            writer.append(1'000'000ULL * i, hft::bench::kBookTickerFrame);
        writer.close();

        return file;
    }();

    return path;
}

/* Cost of the replay loop itself: iterate the mapping and parse each record.
   Divide by items to compare against BM_ParseBookTicker, which is the same parse
   without the iteration. */
void BM_ReplayParse(benchmark::State& state)
{
    const hft::capture::CaptureReader reader(capture_path());

    hft::Message msg{};
    for (auto _ : state)
    {
        for (const hft::capture::CaptureRecord& record : reader)
        {
            hft::feed::parse_book_ticker(record.payload, msg);
            benchmark::DoNotOptimize(msg);
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(reader.record_count()));
}
BENCHMARK(BM_ReplayParse);

/* Full replay hot loop: iterate, parse, publish. Matches what apps/replay does
   between the two rdtsc reads. */
void BM_ReplayParseAndPush(benchmark::State& state)
{
    const hft::capture::CaptureReader reader(capture_path());
    hft::ipc::SpscRing                ring{};
    hft::Message                      out{};
    hft::Message                      msg{};

    for (auto _ : state)
    {
        for (const hft::capture::CaptureRecord& record : reader)
        {
            hft::feed::parse_book_ticker(record.payload, msg);
            msg.timestamp = record.timestamp_ns;
            benchmark::DoNotOptimize(ring.push(msg));
            benchmark::DoNotOptimize(ring.pop(out));
        }
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(reader.record_count()));
}
BENCHMARK(BM_ReplayParseAndPush);

/* Mapping and validating a capture. Startup cost, charged once per replay run. */
void BM_ReaderOpen(benchmark::State& state)
{
    const std::string& path = capture_path();

    for (auto _ : state)
    {
        const hft::capture::CaptureReader reader(path);
        benchmark::DoNotOptimize(reader.record_count());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(kRecords));
}
BENCHMARK(BM_ReaderOpen);

}  // namespace
