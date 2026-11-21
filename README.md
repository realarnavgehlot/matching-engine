# Matching Engine (C++)

A limit order book and matching engine in C++, built step by step from an empty folder.

- Matches orders by **price-time priority**: best price first; at the same price, the earliest order first.
- Supports limit orders, market orders, modify, cancel and Good-Till-Time expiry, and rejects invalid input with a clear reason.
- Time is logical: it only moves when the input says `TIME <t>`, so expiry is as deterministic as everything else.
- **Deterministic:** the same command file always gives exactly the same trades and the same final book.
- One thread owns the book. Every command gets a sequence number and is processed completely before the next one starts.

**Status:** v3 (polish) is in progress: order expiry is done; next is a test for hidden dependence on hash-table order. v2 is done. v1 made it correct, and it is tested with 43 tests, one or more for every rule, plus seeded stress checks compared line by line with an independent reference implementation. v2 made it about 3.5 times faster end to end without changing a single byte of output (see [Performance](#performance) below). The plan is in [`docs/STEPS.md`](docs/STEPS.md).

## Commands

```
BUY <id> <qty> <price>       limit buy (price in paise: 10050 is Rs 100.50)
SELL <id> <qty> <price>      limit sell
BUY ... GTT <time>           limit order that leaves the book when the clock reaches <time>
SELL ... GTT <time>
TIME <time>                  move the logical clock; expire orders whose time has come
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

## Performance

Measured on an AMD Ryzen 5 5600H laptop (Windows 11, g++ 10.3, `-O2`). "End to end" is the real `engine.exe` on 1,000,001 seeded commands, writing 41.7 MB of output to a file. "Book" is the order book alone, timed by `bench.exe` with the input already parsed. Ranges are the best runs over several rounds; differences under about 10% are noise on this machine.

| Version | What changed | End to end | Book | One cancel in a 20,000-order queue |
|---|---|---|---|---|
| v1 | `std::map` of `std::deque` levels; `std::istringstream` parsing; `std::ostream` output | 2.0–2.2 s | 2.3–2.4 M commands/s | about 5 µs |
| Step 15 | Orders in a pool, each level a doubly linked list of slots, id → slot index: cancel and modify no longer search the queue | 1.4–1.9 s | 2.5–3.0 M/s | about 0.2 µs |
| Step 16 | Parser on `std::string_view` and `std::from_chars` (9x faster per line); output buffered in 64 KB blocks, numbers with `std::to_chars` | 0.63–0.80 s | 3.4–4.7 M/s | about 0.2 µs |
| Step 17 | Hash tables sized once from the file's size, instead of growing (and moving every entry) many times | 0.52–0.68 s | 4.6–5.0 M/s | about 0.2 µs |

The v1 end-to-end time was measured with shell timing, the later ones with `tools/time_engine.py`; both are wall-clock times that include starting the program. Adding expiry in v3 cost about 4% end to end on this workload (which has no GTT orders): each order grew from 48 to 64 bytes.

What each step taught:

1. **Measure before optimizing.** The first benchmark showed the book was only about a fifth of the engine's time. Parsing alone took longer than matching.
2. **The right data structure removes a whole cost.** With a linked list per level and an id → slot index, a cancel no longer depends on the queue's length: about 20 times faster in a deep queue. The price: printing a big book got slower, because a linked list jumps around memory and the CPU's cache helps less.
3. **Small allocations add up.** Building a `std::istringstream`, a `std::string` per word and a `std::vector` per line cost about 0.5 µs a line. Views and `std::from_chars` cut that to about 0.07 µs.
4. **Common advice can be wrong for your setup.** `std::ios::sync_with_stdio(false)` made this engine *slower*: on Windows with this compiler it leaves `std::cout` with a 512-byte buffer. It was measured and left out.
5. **Guesses about the cause can be wrong too.** Rare commands took over 10 ms. The guess was the pool's vector growing. Experiments showed it was the hash tables rehashing; reserving them cut the worst limit order from 11–13 ms to 1–2 ms.
6. **Speed work needs a safety net.** Every change was checked with the 36 tests, a byte-for-byte comparison of the full 41.7 MB output, and a differential test against an independent reference engine. The new parser was also compared with the old one on 3,000,000 random lines.

Every number, and how it was measured, is in [`docs/BENCHMARKS.md`](docs/BENCHMARKS.md). To reproduce:

```powershell
python tools/gen_bench.py
g++ -std=c++17 -Wall -Wextra -O2 -o bench.exe bench/bench.cpp src/command.cpp src/order_book.cpp
.\bench.exe build\bench_mixed.txt
python tools/time_engine.py build/bench_mixed.txt
```

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
| `src/order_book.hpp`, `src/order_book.cpp` | The order book: a pool of orders linked into price levels; matching, cancel, modify, and all output |
| `src/output.hpp` | A buffered writer for the output lines (64 KB blocks, numbers with `std::to_chars`) |
| `bench/bench.cpp` | The benchmark: times the book on a command file |
| `tools/gen_bench.py` | Writes the seeded benchmark workloads into `build/` |
| `tools/time_engine.py` | Times the whole `engine.exe` on a command file, from outside |
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
