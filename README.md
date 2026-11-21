# Matching Engine (C++)

A limit order book and matching engine in C++, built step by step from an empty folder.

- Matches orders by **price-time priority**: best price first; at the same price, the earliest order first.
- Supports limit orders, market orders, modify and cancel, and rejects invalid input with a clear reason.
- **Deterministic:** the same command file always gives exactly the same trades and the same final book.
- One thread owns the book. Every command gets a sequence number and is processed completely before the next one starts.

**Status:** v1 is done: 36 tests, one or more for every rule, plus seeded stress checks compared line by line with an independent reference implementation. v2 (speed) is in progress: benchmarks are done; next is O(1) cancel and modify. The plan is in [`docs/STEPS.md`](docs/STEPS.md).

## Commands

```
BUY <id> <qty> <price>       limit buy (price in paise: 10050 is Rs 100.50)
SELL <id> <qty> <price>      limit sell
MARKET_BUY <id> <qty>        buy at any price; any rest is dropped
MARKET_SELL <id> <qty>       sell at any price; any rest is dropped
MODIFY <id> <qty> <price>    change a resting order
CANCEL <id>                  remove a resting order
PRINT                        print the book
```

The exact rules and output format are in [`docs/SPEC.md`](docs/SPEC.md).

## Build and run (Windows, PowerShell)

```powershell
g++ -std=c++17 -Wall -Wextra -O2 -o engine.exe src/main.cpp src/command.cpp src/order_book.cpp
.\engine.exe data\sample.txt
```

## Benchmarks

```powershell
python tools/gen_bench.py
g++ -std=c++17 -Wall -Wextra -O2 -o bench.exe bench/bench.cpp src/command.cpp src/order_book.cpp
.\bench.exe build\bench_mixed.txt
```

On an AMD Ryzen 5 5600H, the v1 book handles about **2.3 million commands per second** on a realistic mix. Every result, and how it was measured, is in [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md).

## Run the tests

```powershell
python run_tests.py
```

This builds the engine, runs every test in `tests/` twice, and compares the output with the expected output. If the two runs differ, the engine isn't deterministic. Each test is also run through `tools/reference_engine.py`, a second, independent implementation of the spec.

Then it runs four stress checks: random commands from fixed seeds (160,000 in total), each run twice, with every output line and every printed book checked, and the whole output compared line by line with the reference engine's (a differential test). Add `--no-stress` to skip them. (If `python` isn't found, try `py run_tests.py`.)

## What's in this folder

| Path | What it is |
|---|---|
| `src/main.cpp` | Reads the command file and runs each command |
| `src/command.hpp`, `src/command.cpp` | Turns one line into a command, and runs it on the book |
| `src/order_book.hpp`, `src/order_book.cpp` | The order book: matching, cancel, modify, and all output |
| `bench/bench.cpp` | The benchmark: times the book on a command file |
| `tools/gen_bench.py` | Writes the seeded benchmark workloads into `build/` |
| `docs/BENCHMARKS.md` | Benchmark results, and how they were measured |
| `docs/SPEC.md` | The rules and the exact input/output format: the contract the tests check |
| `docs/STEPS.md` | The build plan, one small step at a time |
| `docs/PROGRESS.md` | A log of each work session |
| `tests/` | Test cases: `name.in.txt` holds the commands, `name.expected.txt` the exact expected output |
| `tools/gen_commands.py` | Makes a random but repeatable command file from a seed, for stress tests |
| `tools/reference_engine.py` | A second implementation of the spec in Python, used to check the engine's output line by line |
| `data/sample.txt` | A sample command file |
| `run_tests.py` | Builds the engine and runs all the tests and the stress check |
| `CLAUDE.md` | Instructions for Claude Opus, the AI assistant used on this project |
