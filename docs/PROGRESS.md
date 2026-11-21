# Progress log

Newest entry first. Add one at the end of every session: the date, the step, what you did, what's next, and any open questions.

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
