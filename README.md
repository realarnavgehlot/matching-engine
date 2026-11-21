# Matching Engine (C++)

[![tests](https://github.com/realarnavgehlot/matching-engine/actions/workflows/tests.yml/badge.svg)](https://github.com/realarnavgehlot/matching-engine/actions/workflows/tests.yml)

A limit order book and matching engine in C++17: the program at the heart of an exchange, which keeps every buy and sell order that hasn't traded yet and matches them when their prices meet. Built step by step from an empty folder.

- **Price-time priority:** best price first; at the same price, the earliest order first.
- **Order types:** limit, market, IOC (Immediate Or Cancel), FOK (Fill Or Kill), Good-Till-Time expiry, modify and cancel. Invalid input is rejected with a clear reason.
- **Deterministic:** the same command file always gives byte-for-byte the same output. There is no clock and no randomness inside the engine: time only moves when the input says `TIME <t>`.
- **Fast:** about 4–5 million commands per second in the book; 1,000,000 commands end to end, including 42 MB of output, in about 0.4–0.6 s on a laptop. A cancel takes the same time however long its queue is.
- **Checked hard:** 51 hand-written tests, a second engine written independently in Python and compared line by line on hundreds of thousands of seeded random commands, a build with a scrambled hash function to catch hidden dependence on hash-table order, and planted bugs to prove the checks can fail. All of it runs on Linux on every push.

**Status:** complete through v4. The plan, and questions to check understanding of every step, are in [`docs/STEPS.md`](docs/STEPS.md); why it's built this way is in [`docs/DESIGN.md`](docs/DESIGN.md).

## A quick example

[`data/example.txt`](data/example.txt):

```
SELL 1 5 10000
SELL 2 5 10000
SELL 3 4 10100
BUY 4 7 10050
BUY 5 3 9900 GTT 100
PRINT
BUY 6 10 10100 FOK
TIME 100
CANCEL 5
BUY 7 0 10000
PRINT
```

`.\engine.exe data\example.txt` prints:

```
[4] TRADE buy=4 sell=1 qty=5 price=10000
[4] TRADE buy=4 sell=2 qty=2 price=10000
[6] BOOK
  ASK 10000: 2x3
  ASK 10100: 3x4
  BID 9900: 5x3
[7] UNFILLED id=6 qty=10
[8] EXPIRED id=5 qty=3
[9] CANCEL_REJECTED id=5
[10] REJECTED reason=bad_quantity
[11] BOOK
  ASK 10000: 2x3
  ASK 10100: 3x4
```

(The test `tests/readme_example` checks that this output stays exactly right.) Every line starts with the sequence number of the command that caused it. Order 4 buys from order 1 before order 2 (same price, order 1 came first), and pays the sellers' price, not its own limit. The FOK can't get all 10 at Rs 101.00 or less, so nothing trades. At time 100, order 5 expires, so cancelling it later is rejected.

## How it works

```
 asks: std::map<price, Level>  lowest first        bids: std::map<price, Level, std::greater>  highest first
   10000 -> [order 2: 3 left]                         9900 -> [order 5: 3 left]
   10100 -> [order 3: 4 left]

 Each Level is a doubly linked list through the pool:  head <-> ... <-> tail   (front trades first)
 pool:      std::vector<Order>   every resting order, named by its slot number; freed slots are reused
 index:     id -> slot           (hash table, lookup only: never looped over)
 expiries:  (GTT time, seq) -> slot   (std::map: the front expires first)
```

An incoming order trades against the front of the best level on the other side until it's filled or the price no longer crosses; what's left rests at the back of its own level (or is dropped, for market, IOC and FOK orders). Cancel and modify find an order through the index and unlink it in a few steps. [`docs/DESIGN.md`](docs/DESIGN.md) explains each choice and what was rejected.

## Commands

```
BUY <id> <qty> <price>       limit buy (price in paise: 10050 is Rs 100.50)
SELL <id> <qty> <price>      limit sell
BUY ... GTT <time>           limit order that leaves the book when the clock reaches <time>
SELL ... GTT <time>
BUY ... IOC / SELL ... IOC   Immediate Or Cancel: trade what you can now, drop the rest
BUY ... FOK / SELL ... FOK   Fill Or Kill: trade the whole quantity now, or nothing
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
| Step 23 | The whole file read at once and walked with `std::string_view`, instead of `std::getline` per line | about 26% faster than before it (0.37 s vs 0.50 s side by side) | unchanged | about 0.2 µs |

The v1 end-to-end time was measured with shell timing, the later ones with `tools/time_engine.py`; both are wall-clock times that include starting the program. Adding expiry in v3 cost about 4% end to end on this workload (which has no GTT orders): each order grew from 48 to 64 bytes. IOC and FOK in v4 cost nothing measurable.

What each step taught:

1. **Measure before optimizing.** The first benchmark showed the book was only about a fifth of the engine's time. Parsing alone took longer than matching.
2. **The right data structure removes a whole cost.** With a linked list per level and an id → slot index, a cancel no longer depends on the queue's length: about 20 times faster in a deep queue. The price: printing a big book got slower, because a linked list jumps around memory and the CPU's cache helps less.
3. **Small allocations add up.** Building a `std::istringstream`, a `std::string` per word and a `std::vector` per line cost about 0.5 µs a line. Views and `std::from_chars` cut that to about 0.07 µs.
4. **Common advice can be wrong for your setup.** `std::ios::sync_with_stdio(false)` made this engine *slower*: on Windows with this compiler it leaves `std::cout` with a 512-byte buffer. It was measured and left out.
5. **Guesses about the cause can be wrong too.** Rare commands took over 10 ms. The guess was the pool's vector growing. Experiments showed it was the hash tables rehashing; reserving them cut the worst limit order from 11–13 ms to 1–2 ms.
6. **Speed work needs a safety net.** Every change was checked with the tests (36 at the time, 51 now), a byte-for-byte comparison of the full 41.7 MB output, and a differential test against an independent reference engine. The new parser was also compared with the old one on 3,000,000 random lines.

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

This builds the engine, runs every test in `tests/` twice, and compares the output with the expected output. If the two runs differ, the engine isn't deterministic. Each test is also run through `tools/reference_engine.py`, a second, independent implementation of the spec, and through a second build of the engine with a deliberately different hash function (`-DENGINE_SCRAMBLED_HASH`), which must print exactly the same thing: that catches any hidden dependence on hash-table order.

Then it runs four stress checks: random commands from fixed seeds (160,000 in total, including GTT, IOC and FOK orders, `TIME` commands and invalid lines), each run twice and through the scrambled-hash build, with every output line and every printed book checked, and the whole output compared line by line with the reference engine's (a differential test). Add `--no-stress` to skip them. (If `python` isn't found, try `py run_tests.py`.)

The same suite runs on Linux, with a newer g++, on every push: see [`.github/workflows/tests.yml`](.github/workflows/tests.yml) and the badge at the top.

## What's in this folder

| Path | What it is |
|---|---|
| `src/main.cpp` | Reads the whole command file and runs each line as a command |
| `src/command.hpp`, `src/command.cpp` | Turns one line into a command, and runs it on the book |
| `src/order_book.hpp`, `src/order_book.cpp` | The order book: a pool of orders linked into price levels; matching, cancel, modify, and all output |
| `src/output.hpp` | A buffered writer for the output lines (64 KB blocks, numbers with `std::to_chars`) |
| `bench/bench.cpp` | The benchmark: times the book on a command file |
| `tools/gen_bench.py` | Writes the seeded benchmark workloads into `build/` |
| `tools/time_engine.py` | Times the whole `engine.exe` on a command file, from outside |
| `docs/BENCHMARKS.md` | Benchmark results, and how they were measured |
| `docs/DESIGN.md` | Why the engine is built the way it is: each decision, the alternatives rejected, and the costs |
| `docs/SPEC.md` | The rules and the exact input/output format: the contract the tests check |
| `docs/STEPS.md` | The build plan, one small step at a time |
| `docs/PROGRESS.md` | A log of each work session |
| `tests/` | Test cases: `name.in.txt` holds the commands, `name.expected.txt` the exact expected output |
| `tools/gen_commands.py` | Makes a random but repeatable command file from a seed, for stress tests |
| `tools/reference_engine.py` | A second implementation of the spec in Python, used to check the engine's output line by line |
| `data/sample.txt` | A sample command file (no trades) |
| `data/example.txt` | The quick example above, with comments |
| `run_tests.py` | Builds the engine (and the benchmark and the scrambled-hash build) and runs all the tests and stress checks |
| `.github/workflows/tests.yml` | Runs `run_tests.py` on Linux on every push |
| `CLAUDE.md` | Instructions for Claude Opus, the AI assistant used on this project |
