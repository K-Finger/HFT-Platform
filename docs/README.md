# Documentation

- [Architecture](ARCHITECTURE.md) — component design, data flow, latency budget
- [Ingestion](ingestion.md) — the latency-critical path in detail: setup order,
  parsing, publishing, configuration, verification
- [Order book](book.md) — driving a limit order book from snapshots, the integer
  tick boundary, and what the book update costs
- [Replay](replay.md) — recording captures, replaying them for reproducible
  latency numbers, and the capture file format
- [User guide](user_guide.md) — prerequisites, build, host tuning, running the apps
- [Benchmarks](BENCHMARKS.md) — what each benchmark measures, how to reproduce it
