# Matching Engine (C++)

A limit order book and matching engine in C++, built step by step from an empty folder.

- Matches orders by **price-time priority**: best price first; at the same price, the earliest order first.
- Supports limit orders, market orders, modify and cancel, and rejects invalid input with a clear reason.
- **Deterministic:** the same command file always gives exactly the same trades and the same final book.
- One thread owns the book. Every command gets a sequence number and is processed completely before the next one starts.

**Status:** v1 is done: 36 tests, one or more for every rule, plus a seeded 100,000-command stress check. Next: v2 (speed). The plan is in [`docs/STEPS.md`](docs/STEPS.md).

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
g++ -std=c++17 -Wall -Wextra -O2 -o engine.exe src/main.cpp src/order_book.cpp
.\engine.exe data\sample.txt
```

## Run the tests

```powershell
python run_tests.py
```

This builds the engine, runs every test in `tests/` twice, and compares the output with the expected output. If the two runs differ, the engine isn't deterministic. Then it runs a stress check: 100,000 random commands from a fixed seed, run twice, with every output line and every printed book checked. Add `--no-stress` to skip it. (If `python` isn't found, try `py run_tests.py`.)

## What's in this folder

| Path | What it is |
|---|---|
| `src/main.cpp` | Reads the command file and parses each line |
| `src/order_book.hpp`, `src/order_book.cpp` | The order book: matching, cancel, modify, and all output |
| `docs/SPEC.md` | The rules and the exact input/output format: the contract the tests check |
| `docs/STEPS.md` | The build plan, one small step at a time |
| `docs/PROGRESS.md` | A log of each work session |
| `tests/` | Test cases: `name.in.txt` holds the commands, `name.expected.txt` the exact expected output |
| `tools/gen_commands.py` | Makes a random but repeatable command file from a seed, for stress tests |
| `data/sample.txt` | A sample command file |
| `run_tests.py` | Builds the engine and runs all the tests and the stress check |
| `CLAUDE.md` | Instructions for Claude Opus, the AI assistant used on this project |
