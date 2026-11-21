# Benchmarks

How fast the engine is, measured the same way every time, so each speed change in v2 can be compared with the one before.

## How to reproduce

From the `matching-engine` folder:

```powershell
python tools/gen_bench.py
g++ -std=c++17 -Wall -Wextra -O2 -o bench.exe bench/bench.cpp src/command.cpp src/order_book.cpp
.\bench.exe build\bench_mixed.txt 7
.\bench.exe build\bench_deep_cancel.txt 7
.\bench.exe build\bench_deep_modify.txt 7
```

`bench.exe` reads and parses the whole file first, then times only the book. It runs the file on a fresh book several times (throughput), then once more timing every command on its own (latency). Output is formatted as usual but thrown away, so the terminal or disk isn't what's measured.

The clock is in `bench/bench.cpp`, never in the engine (rule 6 in `CLAUDE.md`).

## Workloads

| File | What it is |
|---|---|
| `bench_mixed.txt` | 1,000,001 commands from the stress generator (seed 1): limit, market, modify, cancel and invalid lines, one PRINT at the end |
| `bench_deep_cancel.txt` | 20,000 sells at one price, then every one cancelled in random order (seed 7) |
| `bench_deep_modify.txt` | 20,000 sells at one price, then every one reduced to 5, keeping its place, in random order (seed 8) |

## Reading the numbers

- **Throughput** (commands per second) is the most reliable number. Each benchmark was run 3 times with 7 timed runs each; the tables give the range of the best and median runs.
- **Latency** is per command. On this machine the clock moves in steps of **100 ns**, so values of a few hundred ns are only rough. Latency tables also move by up to about 2x between runs on this laptop, so compare them only loosely.
- This is a laptop with other programs running. Differences under about 10% are noise.

---

## v1 book (2026-11-22, before any speed work)

Machine: AMD Ryzen 5 5600H, Windows 11, TDM-GCC 10.3.0, `-O2`. Book: `std::map` of price levels, each level a `std::deque<Order>`; CANCEL and MODIFY find an order by searching its level from the front.

### Throughput, book only

| Workload | Best | Median |
|---|---|---|
| mixed (1,000,001 commands) | 2.35–2.44 million/s (411–425 ms) | 2.24–2.26 million/s (442–447 ms) |
| deep_cancel (40,001 commands) | 0.33–0.35 million/s (113–120 ms) | 0.26–0.29 million/s (138–153 ms) |
| deep_modify (40,001 commands) | 0.26–0.34 million/s (117–153 ms) | 0.23–0.28 million/s (142–171 ms) |

### Latency per command, mixed workload (ns)

| Type | Count | p50 | p90 | p99 | p99.9 |
|---|---|---|---|---|---|
| all | 1,000,001 | 300 | 800 | 4,200 | 9,500 |
| limit | 557,302 | 300 | 600 | 1,200 | 2,500 |
| market | 103,345 | 500 | 1,100 | 2,000 | 3,900 |
| modify | 119,714 | 200 | 1,900 | 7,000 | 11,300 |
| cancel | 199,914 | 200 | 1,900 | 5,800 | 9,200 |
| reject | 19,725 | 100 | 200 | 300 | 500 |

The single PRINT at the end, of a book with tens of thousands of orders, takes about 4 ms.

### Latency per command, deep workloads (ns)

| Workload | Type | p50 | p90 | p99 |
|---|---|---|---|---|
| deep_cancel | cancel | 5,000–5,400 | 13,500–14,700 | 20,200–21,100 |
| deep_modify | modify | 5,600–6,900 | 13,600–14,600 | 16,700–18,500 |
| both | limit (resting, no trade) | 100 | 100–200 | 200–500 |

### Outside the book

| What | Time for `bench_mixed.txt` |
|---|---|
| Parsing every line (`parseCommand`) | 520–630 ms (about 0.5–0.6 µs per line) |
| Whole `engine.exe`, output written to a file (42 MB) | 1.99–2.21 s |

### What this says

1. **The book is not the bottleneck end to end.** It takes about 0.43 s of the engine's 2.1 s. Parsing (a new `std::istringstream` and `std::string` for every line) takes longer than the book does, and writing the output takes most of the rest. So Step 16 (faster input and output) is worth doing.
2. **CANCEL and MODIFY are slow in deep queues.** In a level of 20,000 orders, a cancel takes about 5 µs at the median, against about 0.2 µs in the mixed workload, because it searches the level from the front. The work grows with the queue's length. This is what Step 15 fixes.
3. **Adding an order is cheap** (about 0.1–0.3 µs) when it doesn't trade.

---

## v2 Step 15: pool and linked lists (2026-11-22)

Same machine, compiler and workloads. Book: orders live in one `std::vector<Order>` (the pool) and are named by slot number; each price level is a doubly linked list of slots; the index maps an id straight to its slot; freed slots are reused. CANCEL and MODIFY unlink an order in a few steps, without searching its level.

The output is unchanged: all 36 tests pass, and the engine matches the reference engine line for line on the 4 stress seeds in `run_tests.py` and on 30 more (seeds 10–39, 20,000 commands each).

### Throughput, book only

| Workload | v1 best | Step 15 best | Step 15 median |
|---|---|---|---|
| mixed | 2.35–2.44 million/s (411–425 ms) | 2.47–3.00 million/s (333–405 ms) | 2.18–2.48 million/s (403–460 ms) |
| deep_cancel | 0.33–0.35 million/s (113–120 ms) | 6.40–6.99 million/s (5.7–6.3 ms) | 5.73–6.53 million/s (6.1–7.0 ms) |
| deep_modify | 0.26–0.34 million/s (117–153 ms) | 5.66–6.33 million/s (6.3–7.1 ms) | 4.19–5.90 million/s (6.8–9.5 ms) |

### Latency per command (ns), v1 → Step 15

| Workload | Type | p50 | p90 | p99 | p99.9 |
|---|---|---|---|---|---|
| mixed | all | 300 → 300 | 800 → 700 | 4,200 → 1,300 | 9,500 → 3,100 |
| mixed | cancel | 200 → 300 | 1,900 → 700 | 5,800 → 1,100 | 9,200 → 1,800 |
| mixed | modify | 200 → 300 | 1,900 → 800 | 7,000 → 1,400 | 11,300 → 2,300 |
| mixed | limit | 300 → 300 | 600 → 600 | 1,200 → 1,000 | 2,500 → 4,300 |
| deep_cancel | cancel | 5,000 → 200 | 13,500 → 300 | 20,200 → 500 | 33,400 → 1,700 |
| deep_modify | modify | 5,600 → 300 | 13,600 → 500 | 16,700 → 700 | 28,300 → 2,600 |

(One latency run per workload; on this laptop these move by up to about 2x between runs, and the clock steps are 100 ns.)

### What this says

1. **Deep queues: about 20 times faster.** A cancel or modify in a 20,000-order level went from about 5 µs to about 0.2–0.3 µs, and no longer depends on the queue's length.
2. **Mixed workload: the slow tail shrank most.** p99 went from 4.2 µs to 1.3 µs, because the cancels and modifies that used to search long queues no longer do. Throughput is a little better, but within the noise of this laptop.
3. **PRINT of a big book got slower** (about 4 ms → about 12 ms, one sample each). A deque keeps a level's orders next to each other in memory; a linked list jumps around the pool, especially once freed slots are reused, so the CPU's cache helps less. PRINT is rare, so this is a good trade, but it's the classic cost of linked lists.
4. **A few single commands still take milliseconds** (mixed `limit` max about 13 ms). The likely cause is the pool's `std::vector` growing: when it runs out of room it copies every order to a bigger array. Reserving space up front would remove that. Not done yet.
