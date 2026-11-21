# Progress log

Newest entry first. Add one at the end of every session: the date, the step, what you did, what's next, and any open questions.

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
