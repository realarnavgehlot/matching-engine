# Matching Engine (C++)

A limit order book and matching engine in C++, built step by step from an empty folder.

- Matches buy and sell limit orders by **price-time priority**: best price first; at the same price, the earliest order first.
- **Deterministic:** the same command file always gives exactly the same trades and the same final book.
- One thread owns the book. Every command gets a sequence number and is processed completely before the next one starts.

**Status:** v0 is done: BUY, SELL, CANCEL and PRINT, with a test for every matching rule (18 tests). Next: v1 (input validation, market orders). The plan is in [`docs/STEPS.md`](docs/STEPS.md).

## Build and run (Windows, PowerShell)

```powershell
g++ -std=c++17 -Wall -Wextra -O2 -o engine.exe src/main.cpp
.\engine.exe data\sample.txt
```

## Run the tests

```powershell
python run_tests.py
```

This builds the engine, runs every test in `tests/` twice, and compares the output with the expected output. If the two runs differ, the engine isn't deterministic. (If `python` isn't found, try `py run_tests.py`.)

## What's in this folder

| Path | What it is |
|---|---|
| `src/` | The engine's C++ code |
| `docs/SPEC.md` | The rules and the exact input/output format: the contract the tests check |
| `docs/STEPS.md` | The build plan, one small step at a time |
| `docs/PROGRESS.md` | A log of each work session |
| `tests/` | Test cases: `name.in.txt` holds the commands, `name.expected.txt` the exact expected output |
| `data/sample.txt` | A sample command file |
| `run_tests.py` | Builds the engine and runs all the tests |
| `CLAUDE.md` | Instructions for Claude Opus, the AI assistant used on this project |
