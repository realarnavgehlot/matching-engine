# Progress log

Newest entry first. Add one at the end of every session: the date, the step, what you did, what's next, and any open questions.

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
