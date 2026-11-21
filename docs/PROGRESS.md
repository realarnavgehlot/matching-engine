# Progress log

Newest entry first. Add one at the end of every session: the date, the step, what you did, what's next, and any open questions.

## 2026-11-22: Step 24 done, v4 complete (by Claude, while Arnav was away)

- README rewritten at the top: what a matching engine is, a test badge, a quick example (`data/example.txt`) with its real output and an explanation, a short "how it works" diagram of the data structures, the Step 23 row in the performance table.
- `tests/readme_example`: the README's example as a test (51 tests now), so the README can't silently go out of date.
- Every step from 19 to 24 was tested (50–51 tests, 4 stress seeds against the reference and the scrambled-hash build, 30 extra seeds for Steps 21) and pushed on its own; CI on Linux passes.
- **Next (for Arnav):** nothing is planned after v4. Before adding anything, read the code with the questions in `docs/STEPS.md` (Steps 13–24 have question lists) until every line can be explained, and read `docs/DESIGN.md`. All code from Step 1 on was written by Claude.
- **Open questions:** (1) `CLAUDE.md` still says Arnav writes the engine code; decide whether to keep that rule going forward. (2) Packing `Order` (64 bytes) might win back the ~4% that expiry cost; only if a measurement says so.

## 2026-11-22: Steps 22 and 23 done, CI on Linux and faster input (by Claude, while Arnav was away)

- Step 22: `.github/workflows/tests.yml` runs `python run_tests.py` on ubuntu-latest (newer g++) on every push and pull request. It now passes.
  - The first run failed: on Linux the benchmark binary was named `bench`, which clashes with the `bench/` folder. Renamed to `bench_engine` on Linux (`bench.exe` on Windows is unchanged); Linux build outputs added to `.gitignore`.
  - The second run failed in the workflow itself: under GitHub's `bash -e`, a `grep` that found nothing to report ended the step with an error. Fixed; failing lines are now shown as annotations on the run's page (the full log needs the owner's login).
  - `order_book.hpp` includes `<cstddef>` for `std::size_t` instead of relying on another header.
- Step 23: `main()` reads the whole file at once (binary mode) and walks it with `std::string_view` lines instead of `std::getline`. Output byte-identical (including a `\r\n` file with no final newline); end to end about 26% faster (12 alternating runs: median 0.503 → 0.372 s).
- **Next:** Step 24 (README polish).

## 2026-11-22: Step 21 done, IOC and FOK (code written by Claude, at Arnav's request, while he was away)

- Planned v4 in `docs/STEPS.md` (Steps 21–24).
- `docs/SPEC.md` rules 18–20: `IOC` trades what it can, drops the rest (`UNFILLED`); `FOK` trades its whole quantity at once or nothing (`UNFILLED` with the full quantity). Both use up their id and never rest.
- Engine: a `TimeInForce` enum (GoodTillCancel, GoodTillTime, ImmediateOrCancel, FillOrKill) replaces the optional GTT time in `Command` and `limit()`. `canFill()` walks the crossing orders best first and stops when there's enough (no per-level totals, which could overflow).
- Reference engine: FOK adds up all crossing quantity instead. Generator: IOC/FOK behind `with_ioc_fok` (off for the benchmark; checksum unchanged).
- 7 new tests (50 in total); all pass on the engine, the reference and the scrambled build; 4 stress seeds and 30 extra seeds match the reference (about 1,400 IOC/FOK orders per 20,000-command seed).
- Planted 4 bugs: the differential test caught 3. It missed `>` instead of `>=` in `canFill()` (an exact fit), which random orders almost never hit; the hand-written tests `fok_fills` and `fok_killed` do catch it.
- Speed: no measurable change (end-to-end median 0.834 → 0.809 s, within noise).
- **Next:** Step 22 (CI on GitHub Actions).

## 2026-11-22: Step 20 done, v3 complete (written by Claude, at Arnav's request)

- `docs/DESIGN.md`: every design decision (paise, logical time, one thread, `std::map` levels, templates, the pool and linked lists, hash tables only for lookup, the expiry map, MODIFY/market/rejection rules, parser and output buffer, the testing strategy, benchmarking method) with why, what was rejected, and the cost. Plus known limits. Linked from the README.
- **Next:** v4 (planned in `docs/STEPS.md`).
- **Open question:** none.

## 2026-11-22: Step 19 done, hash-order test (code written by Claude, at Arnav's request)

- `src/order_book.hpp`: the hash tables take an `IdHash` type. Normally `std::hash`; with `-DENGINE_SCRAMBLED_HASH` it's MurmurHash3's fmix64, and the size hint is skipped, so the tables' internal order is completely different.
- `run_tests.py` builds `engine_scrambled.exe` too and requires identical output from both builds on all 43 tests and the 4 stress seeds. All pass.
- Proof: planted a loop over `index_` in `setTime()`. The hand-written test `expire_partly_filled` still passed with the normal build, but the scrambled build differed; `expire_order_and_ties` caught it the other way round; the stress files caught it both ways.
- **Next:** Step 20 (`docs/DESIGN.md`).
- **Open question:** none.

## 2026-11-22: Step 18 done, order expiry (code written by Claude, at Arnav's request)

- Planned v3 in `docs/STEPS.md`: Step 18 expiry, Step 19 hash-order test, Step 20 `docs/DESIGN.md`.
- `docs/SPEC.md`: `TIME <t>` moves a logical clock (starts at 0, never backwards: `time_backwards`); `BUY`/`SELL ... GTT <t>` (must be in the future: `bad_expiry`); `EXPIRED id=.. qty=..` in order of (GTT time, sequence number); MODIFY keeps the GTT time. Rules 13–17.
- Engine: `Command` and `Order` carry `std::optional<std::int64_t> expiry`; the book keeps `now_` and `expiries_` (a `std::map` keyed by (GTT time, sequence number)); filled, cancelled and moved orders are removed from it. Reference engine: scans and sorts on each TIME instead.
- 7 new tests (43 in total); the stress generator adds TIME and GTT (with `with_expiry=False` the files are byte-identical to v2, checked by checksum, so the benchmark stays comparable). 43/43 tests, 4 stress seeds and 30 extra seeds match the reference; each 20,000-command seed has about 870 expiries.
- Planted 4 expiry bugs; the differential test caught all 4. Leaving a filled order in `expiries_` made the engine hang (a stale slot corrupted the linked lists).
- Cost: end to end about 4% slower on the old workload (12 alternating runs: median 0.717 → 0.743 s), probably because `Order` grew from 48 to 64 bytes.
- **Next:** Step 19 (hash-order test).
- **Open question:** packing the GTT time into fewer bytes might win back the 4%. Not worth it unless a measurement says so.

## 2026-11-22: Step 17 done, v2 complete (code written by Claude, at Arnav's request)

- Tested the Step 15 open question with four throwaway builds: the rare 10+ ms commands came from the hash tables (`index_`, `usedIds_`) rehashing, not from the pool's vector growing. Reserving the pool did nothing; reserving the hash tables cut the worst limit order from 11–13 ms to about 1 ms.
- `OrderBook(std::ostream&, std::size_t expectedOrders = 0)` reserves the hash tables; `main()` guesses from the file size (bytes / 16); `bench.exe` passes its command count. Small files pay nothing (sample.txt about 8 ms either way).
- Alternating 12 runs each against Step 16: end-to-end median 0.652 → 0.605 s, worst 0.855 → 0.676 s. Book throughput about 4.6–5.0 million/s (best).
- README has a Performance section: a table from v1 to Step 17 and six lessons. `docs/BENCHMARKS.md` has the Step 17 experiment.
- Output byte-identical; 36/36 tests and the differential check pass.
- **Next:** v3 (polish), planned in detail later. Before that: read the code and answer the questions in `docs/STEPS.md` for Steps 13–17.
- **Open question:** none.

## 2026-11-22: Step 16 done, faster input and output (code written by Claude, at Arnav's request)

- `tools/time_engine.py`: times the real `engine.exe` end to end from outside (output to a file).
- Tried `std::ios::sync_with_stdio(false)` first: it made the engine **slower** (512-byte buffer on Windows with this compiler). Not kept.
- New parser in `src/command.cpp`: splits each line into `std::string_view` words and uses `std::from_chars`; no `std::istringstream`, `std::string` or `std::vector` per line. 520–710 → 64–72 ns per line. Compared with the old parser on 3,000,000 random lines: 0 differences (and a planted difference was caught).
- `src/output.hpp`: an `Output` class that buffers 64 KB, formats numbers with `std::to_chars`, and writes in big blocks. `OrderBook` now writes through it; `main()` and the benchmark call `book.flush()`.
- End to end on 1,000,001 commands: about 1.4–1.9 s → 0.63–0.80 s (about 2.5x). Book throughput: about 2.5 → 3.2–4.7 million commands per second. Output byte-identical after every change; 36/36 tests and the differential check pass.
- **Next:** Step 17 (README section with the before/after story).
- **Open question:** the pool still grows by copying (the rare multi-millisecond command). Try `pool_.reserve()` and measure, maybe as part of Step 17 or v3.

## 2026-11-22: Step 15 done, O(1) cancel and modify (code written by Claude, at Arnav's request)

- Rewrote the storage in `src/order_book.hpp/.cpp`: orders live in a pool (`std::vector<Order>`), named by slot number; each price level is a doubly linked list of slots (`head`, `tail`, and `prev`/`next` in each order); the index maps id → slot; freed slots are reused from a free list. `Location` and the `std::deque` are gone. `command.cpp` and `main.cpp` didn't change.
- Output unchanged: 36/36 tests, the 4 differential stress seeds, and 30 extra seeds (10–39) all match the reference engine.
- Benchmarks (`docs/BENCHMARKS.md`): deep_cancel and deep_modify about 20 times faster (a cancel in a 20,000-order level: about 5 µs → about 0.2 µs); mixed p99 4.2 µs → 1.3 µs; mixed throughput slightly better but within noise. PRINT of a big book got slower (about 4 → 12 ms): linked lists have poor cache locality.
- **Next:** Step 16 (faster input and output), where most of the end-to-end time is.
- **Open question:** a few single commands take milliseconds, probably the pool's vector growing and copying. `pool_.reserve(...)` might fix it; measure before deciding.

## 2026-11-22: Step 14 done, benchmarks (code written by Claude, at Arnav's request)

- Moved line parsing out of `main()` into `src/command.hpp` and `src/command.cpp` (`isCommandLine`, `parseCommand`, `runCommand`), so the engine and the benchmark share it. All tests and the differential check still pass. The build command now has three `.cpp` files.
- `bench/bench.cpp`: parses the file first, then times only the book; throughput over several runs, then latency per command, broken down by command type. Output is formatted and thrown away. It reports the clock's resolution (100 ns here), because small latencies are rough below that.
- `tools/gen_bench.py`: three seeded workloads (mixed, 1,000,001 commands; deep_cancel and deep_modify, 20,000 orders at one price).
- `run_tests.py` now also builds `bench.exe`, so a change in `src/` can't break it unnoticed.
- v1 numbers in `docs/BENCHMARKS.md`: book about 2.3 million commands per second on mixed; whole `engine.exe` about 2.1 s for 1 million commands, so parsing and output cost far more than the book; one CANCEL in a 20,000-order level about 5 µs.
- **Next:** Step 15 (O(1) cancel and modify), then measure again.
- **Open question:** the laptop's numbers vary by about 10% between runs. Good enough to see 2x changes, not small ones.

## 2026-11-22: Step 13 done, differential test (code written by Claude, at Arnav's request)

- Planned v2 in `docs/STEPS.md` (Steps 13–17): differential test first, then benchmarks, then O(1) cancel/modify, then faster I/O if the numbers say so.
- `tools/reference_engine.py`: a second implementation of the spec in Python. It stores no queue order (each order has a priority number instead) and finds the best price with min/max, so it doesn't share the C++ engine's data-structure bugs. It agrees with all 36 expected outputs.
- `run_tests.py` now runs four seeded stress files (seed 1: 100,001 commands; seeds 2–4: 20,001 commands with a PRINT every 250) and fails on the first line where the engine and the reference differ. Whole run: about 9 seconds.
- Planted 5 bugs in throwaway copies of the engine (equal-qty MODIFY losing its place, no trade at an exactly touching price, LIFO queue, index not cleared on fill, price-change MODIFY keeping its place). The differential test first caught 4; the generator almost never sent a same-price, same-qty MODIFY. Changed the generator to aim MODIFY at the rule 11/12 edges; now all 5 are caught.
- **Next:** Step 14 (benchmarks), recording the v1 numbers before any speed change.
- **Open question:** none.

## 2026-11-22: v1 done, Steps 8–12 (code written by Claude, at Arnav's request)

- `docs/SPEC.md` now defines v1: input rules, the `REJECTED reason=...` line and the order of checks, `MARKET_BUY`/`MARKET_SELL` with `UNFILLED`, and `MODIFY` (rules 10–12). These rules follow common exchange practice; change them if you disagree.
- Code split into `src/main.cpp` (reads and parses lines), `src/order_book.hpp` and `src/order_book.cpp` (the book). The build command now lists both `.cpp` files.
- Numbers are parsed with `std::from_chars`, so `-1`, `5x`, `10.50` and numbers too big for 64 bits are caught. The v0 hazard (`SELL 9 5` read as price 0, matching every bid) is now `bad_format`.
- 18 new tests (36 in total), plus a stress check: `tools/gen_commands.py` with seed 1 makes 100,001 commands; the engine runs them twice in a few seconds with identical output, about 48,000 trades, and every printed book sorted and never crossed. Checked that the stress checker does catch crossed, unsorted or malformed output.
- **Next:** read `order_book.cpp` and the v1 questions in `docs/STEPS.md`; then plan v2 (speed).
- **Open question:** the stress check proves the output is well formed, not that every trade is right. A differential test against a second, independent implementation (planned for v2) would close that gap.

## 2026-11-22: Step 7 done, v0 is on GitHub

- `git init -b main` inside `matching-engine`, so the project has its own repository (separate from the one in `C:\Users\gehlo`). First commit: `736215c`.
- Public repository: https://github.com/realarnavgehlot/matching-engine. The remote URL includes `realarnavgehlot@` so Git signs in as that account, not arnavgehlot-bitspilani.
- **Next:** read `src/main.cpp` until every line can be explained (questions in the entry below), then plan v1.
- **Open question:** the commit email may belong to the arnavgehlot-bitspilani account. If commits don't show on realarnavgehlot's profile, add that email to realarnavgehlot on GitHub, or set `git config user.email` in this repository.

## 2026-11-22: Steps 2–6 done (code written by Claude, at Arnav's request)

- `src/main.cpp` now has an `OrderBook` class: bids and asks as `std::map` from price to a `std::deque<Order>`, plus an `unordered_map` from id to location, used only for lookup in CANCEL.
- BUY and SELL share one matching function (the Step 4 bonus), written as a template so it works on both maps.
- 16 new tests in `tests/`, covering every matching rule in `docs/SPEC.md` and comment/blank-line handling. `python run_tests.py`: 18/18 pass, and both runs of every test gave the same output.
- Unknown commands print a warning to `std::cerr` and are skipped (v0 assumes valid input).
- **Before Step 7:** Arnav reads `main.cpp` and can explain it, especially: why bids use `std::greater`, why `index_.erase(resting.id)` comes before `pop_front()`, why the trade price is `levelPrice`, and why the book can never be "crossed" after a command.
- **Next:** Step 7 (own Git repository, then GitHub).
- **Open question:** none.

## 2026-11-22: Step 1 done (code written by Claude, at Arnav's request)

- `src/main.cpp` reads the command file given as `argv[1]`, skips blank and `#` lines, numbers the rest from 1, and prints `[seq] got ...` for each. On `data/sample.txt` it prints the 6 expected lines.
- Errors: no argument, or a file that can't be opened, prints to `std::cerr` and exits with code 1. Both checked.
- Switched from `using namespace std;` to writing `std::` everywhere.
- **Before Step 2:** Arnav reads `main.cpp` line by line and can explain each part, especially why `argc < 2`, why `while (std::getline(...))` stops, and what `in >> id >> qty >> price` does.
- **Next:** Step 2 (store orders in the book and PRINT it).
- **Open question:** a line with only spaces is not "blank" by the current check, so it gets a sequence number. Fine for v0 (input is assumed valid); revisit in v1.

## 2026-11-22: Step 0 done

- g++ is TDM-GCC 10.3.0 (C++17 works). Wrote `src/main.cpp`, built `engine.exe`, it prints `matching engine v0`.
- **Next:** Step 1 (read the command file).

## 2026-11-22: Fresh start

- Restarted the project from an empty folder. Arnav writes the engine code; Claude Opus teaches and reviews (see `CLAUDE.md`).
- Set up: README, CLAUDE.md, docs (SPEC, STEPS, PROGRESS), the two contract tests in `tests/`, `data/sample.txt` and `run_tests.py`. No engine code yet.
- **Next:** Step 0 (tools and hello world).
