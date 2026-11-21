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
4. **A few single commands still take milliseconds** (mixed `limit` max about 13 ms). The likely cause is the pool's `std::vector` growing: when it runs out of room it copies every order to a bigger array. Reserving space up front would remove that. Not done yet. *(Step 17 tested this guess: it was wrong. The cause was the hash tables.)*

---

## v2 Step 16: faster input and output (2026-11-22)

Same machine, compiler and workloads. End-to-end times come from `tools/time_engine.py`, which runs the real `engine.exe` on `build/bench_mixed.txt` (1,000,001 commands) with its 41,658,934 bytes of output written to a file; 3 rounds of 5 runs, best and median per round. After every change the output was compared byte for byte with the output before Step 16, and was identical.

### Each change, measured end to end

| Change | Best | Median | Kept? |
|---|---|---|---|
| Before Step 16 | 1.43–1.93 s | 1.66–2.02 s | |
| `std::ios::sync_with_stdio(false)` | 2.00–2.37 s | 2.27–2.67 s | **No: slower** |
| New parser (`std::string_view` words, `std::from_chars`; no `std::istringstream`, `std::string` or `std::vector` per line) | 0.98–1.11 s | 1.05–1.17 s | Yes |
| + `Output` buffer (64 KB, numbers with `std::to_chars`, one big write per 64 KB) | 0.71–0.79 s | 0.74–0.86 s | Yes |
| + `sync_with_stdio(false)` again | 0.66–0.86 s | 0.71–0.96 s | No: no clear difference |
| Final build, measured again | **0.63–0.80 s** | **0.66–0.84 s** | |

**About 2.5 times faster end to end.**

Why `sync_with_stdio(false)` made things slower here: with sync on, `std::cout` writes through C's `fwrite`, which has its own buffer. With sync off, libstdc++ on Windows uses its own buffer of `BUFSIZ` bytes, which is only 512 bytes with this compiler, so the output goes to the operating system in many tiny writes. Once the engine writes 64 KB blocks itself, the setting stops mattering. Common advice, measured, didn't hold on this setup.

### Parsing

| | Before | After |
|---|---|---|
| `parseCommand`, mixed workload | 520–710 ns per line | 64–72 ns per line (**about 9 times faster**) |

The new parser was checked against the old one on 3,000,000 random lines (real command words, edge-case numbers such as `+5`, `007`, `10.50` and 2^63, every kind of whitespace, and random bytes): 0 differences. The same check found 292,910 differences when `\v` was planted as "not a space", so it can catch a difference when there is one.

### Book throughput (bench.exe)

The book's own time also dropped, because formatting each output line is part of its work.

| Workload | Step 15 best | Step 16 best | Step 16 median |
|---|---|---|---|
| mixed | 2.47–3.00 million/s | 3.35–4.66 million/s (214–298 ms) | 3.22–3.62 million/s (276–310 ms) |
| deep_cancel | 6.40–6.99 million/s | 6.42–8.40 million/s | 5.54–7.72 million/s |
| deep_modify | 5.66–6.33 million/s | 8.68–9.18 million/s | 6.40–8.22 million/s |

### Latency per command, mixed (ns), Step 15 → Step 16

| Type | p50 | p90 | p99 | p99.9 |
|---|---|---|---|---|
| all | 300 → 200 | 700 → 500 | 1,300 → 1,000 | 3,100 → 2,300 |
| limit | 300 → 200 | 600 → 500 | 1,000 → 800 | 4,300 → 3,600 |
| cancel | 300 → 200 | 700 → 600 | 1,100 → 1,000 | 1,800 → 1,800 |

(One run each; the clock steps are 100 ns.)

### Where the time goes now (mixed, end to end, about 0.7 s)

| Part | About |
|---|---|
| The book, including formatting output | 0.25–0.30 s |
| Parsing | 0.07 s |
| Everything else: starting the process, reading 18 MB line by line with `std::getline`, and writing 42 MB to disk | 0.3–0.4 s |

Reading the whole file at once instead of line by line might save a little more, but this is now small next to the book. Step 16 stops here.

---

## v2 Step 17: what caused the rare slow commands (2026-11-22)

Since Step 15, a few single commands on the mixed workload took over 10 ms. Two kinds of growth could cause it: the pool's `std::vector` copying every order when it runs out of room, or the hash tables (`index_`, `usedIds_`) rehashing, which moves every entry into a bigger table. Four throwaway builds of the Step 16 book, each run 3 times on `bench_mixed.txt`:

| Build | Book throughput (best) | Slowest single limit order | Slowest market order |
|---|---|---|---|
| No change | 3.00–3.22 million/s | 10.9–12.7 ms | 0.5–0.7 ms |
| `pool_.reserve(2^21)` | 3.02–3.48 million/s | 11.5–15.1 ms | 0.7–1.1 ms |
| `index_` and `usedIds_` `.reserve(2^21)` | 3.91–4.33 million/s | 0.56–1.10 ms | 0.05–0.33 ms |
| Both | 4.16–4.54 million/s | 0.12–1.87 ms | 0.16–0.51 ms |

**The cause was the hash tables, not the pool.** The Step 15 guess was wrong. `usedIds_` holds every id ever accepted, about 660,000 here, and each time it grows it moves all of them. Reserving the pool made no clear difference, so it isn't reserved.

Reserving 2^21 entries always would cost small files start-up time: `data/sample.txt` took 7.6 ms → 14.3 ms (best of 30). So `OrderBook` takes an optional `expectedOrders` hint, and `main()` guesses it from the file's size (bytes / 16, since a command line is about 16–20 bytes). The benchmark passes its exact command count. A wrong guess only costs speed: the output can't change, because the hash tables are never looped over.

### Result

| | Step 16 | Step 17 (with the hint) |
|---|---|---|
| End to end, mixed, 12 runs each **alternating** old and new | best 0.568 s, median 0.652 s, worst 0.855 s | best 0.523 s, median 0.605 s, worst 0.676 s |
| `data/sample.txt` end to end (best of 30) | 7.6 ms | 8 ms (no extra cost) |
| Book throughput, mixed, best | 3.35–4.66 million/s | 4.60–4.96 million/s |
| Book throughput, mixed, median | 3.22–3.62 million/s | 3.92–4.31 million/s |
| Slowest single limit order, mixed | 11–13 ms | 1.3–2.1 ms |
| deep_modify, best | 8.68–9.18 million/s | 10.07–10.25 million/s |
| deep_cancel, best | 6.42–8.40 million/s | 5.40–7.26 million/s (within noise: these tables are small) |

Running the two versions alternately matters: on this laptop, separate rounds of 5 runs moved by more than the difference being measured (0.64–0.73 s vs 0.63–0.80 s), but alternating 12 runs each showed a steady gain of about 7% on the median.

Latency, mixed (ns), two runs: p50 200, p90 500, p99 1,000–1,100, p99.9 2,300–3,000; cancel p99 1,000–1,100; limit p99 700. The largest single numbers left are the one PRINT of a big book (6–8 ms) and a few commands of 1–2 ms, probably the operating system interrupting the program.

Output byte-identical to Step 16; 36/36 tests and the differential check pass.

---

## v3 Step 18: the cost of expiry (2026-11-22)

Expiry adds a GTT time (`std::optional<std::int64_t>`, 16 bytes) to every order, so `Order` grew from 48 to 64 bytes, and a fill or cancel now checks whether the order has a GTT time. The benchmark workload is unchanged (generated with `with_expiry=False`, checked by checksum), so it has no GTT orders: this measures what expiry costs code that doesn't use it.

| | Step 17 | Step 18 |
|---|---|---|
| End to end, mixed, 12 runs each alternating | best 0.660 s, median 0.717 s | best 0.686 s, median 0.743 s |
| Book, mixed, best of 5, four paired rounds | 3.35, 4.14, 4.44, 4.03 million/s | 3.29, 4.14, 3.67, 3.40 million/s |

About 4% slower end to end; the book-only pairs range from equal to about 15–20% slower, which is mostly noise. Packing the GTT time into fewer bytes might win some of it back; not done, because the gain would be near the noise.

---

## v4 Step 21: IOC and FOK (2026-11-22)

No measurable change on the benchmark workload (which has no IOC or FOK orders): end to end, 10 alternating runs each, median 0.834 s before and 0.809 s after, within noise.

---

## v4 Step 23: read the whole file at once (2026-11-22)

`main()` used `std::getline`, which copies every line into a `std::string`. It now reads the whole file into one `std::string` with a single `read()`, and walks it with `std::string_view`s, one per line, without copying. The file is opened in binary mode, so Windows `\r\n` endings keep their `\r`; the parser already treats `\r` as a space, so nothing changes.

Output byte-identical to before on `bench_mixed.txt`, `stress_seed2.txt`, `data/sample.txt`, and a Windows-style file with `\r\n` endings and no newline after the last line. All 50 tests and the differential check pass.

| End to end, mixed, 12 runs each alternating | Best | Median |
|---|---|---|
| `std::getline` (before) | 0.499 s | 0.503 s |
| Whole file at once (Step 23) | 0.363 s | 0.372 s |

**About 26% faster end to end.** (The machine was quieter during these runs than during earlier rounds, so both numbers are lower than in the tables above; only the side-by-side comparison counts.) Per line, `std::getline` cost about 0.13 µs more than finding the next `'\n'` in memory: the copy into a `std::string`, and the stream machinery around each call.
