# Phase 2 Checklist — Infrastructure Depth

> Prerequisite: Phase 1 complete (L2 feed + MarketDataBook + OFI signal + virtual P&L).

---

## Huge Pages

- [ ] Check baseline: `grep Huge /proc/meminfo` — confirm `HugePages_Total` is 0
- [ ] Reserve pages: `echo 64 | sudo tee /proc/sys/vm/nr_hugepages`
- [x] In `shared_memory.cpp` (this repo's `shm_provider.cpp`): `MAP_HUGETLB | MAP_HUGE_2MB | MAP_POPULATE`, with a 4KB fallback when huge pages aren't reserved
- [ ] ~~Round allocation size up to 2MB boundary~~ — not needed: the ring is 64KB (1024 slots × 64B), under one 2MB page, and the kernel rounds a `MAP_HUGETLB` mapping up to the page size itself
- [ ] Verify: re-run `grep Huge /proc/meminfo`, confirm `HugePages_Free` drops
- [ ] Benchmark: `perf stat -e dTLB-load-misses` before and after, put delta in README

---

## End-to-End TSC Timestamps

- [x] ~~Add `uint64_t t_recv` field to `Message`, stamp with `rdtsc()`~~ — not done as written: `rdtsc()` is documented as not comparable across cores (`tsc.hpp`), and ingestion/consumer are pinned to different cores (2 and 3), so a raw TSC delta across the ring would be invalid. Reused the existing `Message.timestamp` (wall-clock ns, already documented as cross-process-safe) instead.
- [x] Add a second `LatencyHistogram` in `consumer.cpp` for end-to-end latency (`wire_latency_ns`, measuring exchange-recv-to-book-applied)
- [x] Print p50 / p99 / p999 alongside the existing bucket histogram (`LatencyHistogram::percentile()`, now printed by every `print()` call)
- [ ] Put the p99 number in the README — needs a live run against the exchange feed

---

## Rust Logging Daemon

- [ ] `cargo new hft-logger` at repo root
- [ ] Add `nix` crate to `Cargo.toml`
- [ ] Mirror `Message` struct in Rust with `#[repr(C, align(64))]` — fields must match C++ layout exactly
- [ ] Add a **second** SPSC ring buffer in `shm_provider` (separate `/dev/shm` path) — ingestion writes to both, Rust reads the second one, C++ consumer is untouched
- [ ] In Rust: open the shm path with `nix::sys::mman::mmap`, cast to the ring buffer layout
- [ ] Spin-poll for new messages, write raw bytes to a binary log file
- [ ] Wire the Rust build into the top-level `Makefile` or `CMakeLists.txt`
- [ ] Smoke test: run ingestion + logger, confirm log file grows and `hexdump` shows recognizable price values

---

## Purpose

**Huge pages + TSC timestamps**: give you real latency numbers (p50/p99/p999) to defend in interviews. Without these you can describe the architecture but can't quantify it.

**Rust logging daemon**: demonstrates polyglot systems programming — unsafe Rust binding directly to a C++ shared memory layout. Shows the architecture has a separate fast path (C++ signal) and throughput path (Rust logger) that don't interfere. This is how real firms structure it.

Estimated time: ~10 hours total. Rust daemon is the bulk (~6h). Struct layout mirroring is the step most likely to produce silent bugs — verify with `hexdump` before trusting the log data.
