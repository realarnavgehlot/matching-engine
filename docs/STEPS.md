# Build plan: v0, one step at a time

Each step is small (about 30–90 minutes) and ends with a check you can run. Do them in order. When a step's check passes, tick it here and add a line to `docs/PROGRESS.md`.

Three habits:

- **Write the code yourself.** Stuck for 15 minutes? Ask Claude for a *hint*, not the answer.
- **Rebuild and run after every small change.** Small changes mean small bugs.
- **Predict, then check.** Work out each example's output on paper before you run it.

## Timeline

- **Nov 22–29:** Steps 0–2, in short sessions. Exam prep comes first.
- **Nov 30 – Dec 15:** compre exams. Project paused.
- **Dec 16–23:** Steps 3–7. v0 goes on GitHub.
- **Dec 24:** send the update with the GitHub link.

## v0 checklist

- [x] Step 0: Tools and hello world
- [x] Step 1: Read the command file
- [x] Step 2: Store orders in the book and PRINT it
- [x] Step 3: Match an incoming BUY
- [x] Step 4: Match an incoming SELL
- [x] Step 5: CANCEL
- [x] Step 6: A test for every rule
- [x] Step 7: Git and GitHub

---

## Step 0: Tools and hello world (15–30 min)

**Goal:** prove your compiler works.

1. Open the `matching-engine` folder in File Explorer, click the address bar, type `powershell` and press Enter. A terminal opens in this folder. (Or open the folder in VS Code and use Terminal → New Terminal.)
2. Run `g++ --version`. It should print a version number, 7 or newer. Your laptop seems to have TDM-GCC and MinGW installed, so this may already work. If it says "not recognized", or the version is below 7, tell Claude.
3. Create `src/main.cpp`:

   ```cpp
   #include <iostream>

   int main() {
       std::cout << "matching engine v0\n";
       return 0;
   }
   ```

4. Build: `g++ -std=c++17 -Wall -Wextra -O2 -o engine.exe src/main.cpp`
5. Run: `.\engine.exe`

**Done when:** it prints `matching engine v0`.

---

## Step 1: Read the command file (45 min)

**Goal:** read `data/sample.txt` line by line, give each command its sequence number, and print what you understood.

**Learn:** `int main(int argc, char* argv[])` (to read the file name you pass), `std::ifstream`, `std::getline`, `std::istringstream`, and `>>` to read words and numbers.

**Do:**

- The program takes the command file as its first argument: `.\engine.exe data\sample.txt`. If no file is given, or it can't be opened, print an error and `return 1`.
- Skip blank lines and lines starting with `#`.
- Give every other line the next sequence number, starting at 1.
- For now, print each command with its fields, like `[1] got BUY id=1 qty=5 price=9900` and `[6] got PRINT`.

**Done when:** running on `data/sample.txt` prints 6 lines, numbered 1 to 6, with the right numbers in each.

<details><summary>Hint</summary>

Read a whole line with `std::getline(file, line)`. Put it in `std::istringstream in(line);` and read the first word with `in >> cmd;`. If `cmd` is `"BUY"` or `"SELL"`, read three numbers with `in >> id >> qty >> price;`. Use `std::uint64_t` for the id and `std::int64_t` for qty and price (both from `<cstdint>`).

</details>

---

## Step 2: Store orders in the book and PRINT it (60 min)

**Goal:** BUY and SELL add orders to the book (no matching yet), and PRINT prints the book in the exact format in `docs/SPEC.md`.

**Learn:** `struct`, `std::deque`, `std::map`, and how `std::greater` reverses a map's order.

**Do:**

- Make an `Order` with the id, the remaining quantity, and the sequence number (useful for debugging).
- Keep bids and asks as maps from price to a queue of orders. Bids must go highest price first; asks lowest price first.
- BUY adds the order to the back of the queue at its price in the bids; SELL does the same in the asks.
- PRINT prints `[seq] BOOK` and the levels exactly as the spec says. Remove the `got ...` lines from Step 1.

**Done when:** `.\engine.exe data\sample.txt` prints exactly:

```
[6] BOOK
  ASK 10000: 5x3
  ASK 10100: 4x10
  BID 9900: 1x5 2x7
  BID 9800: 3x4
```

No prices in `data/sample.txt` cross (the best bid, 9900, is below the best ask, 10000), so this output is correct even before matching exists.

<details><summary>Hint</summary>

```cpp
struct Order {
    std::uint64_t id;
    std::int64_t qty;   // remaining quantity
    std::uint64_t seq;
};

std::map<std::int64_t, std::deque<Order>, std::greater<std::int64_t>> bids;  // highest price first
std::map<std::int64_t, std::deque<Order>> asks;                              // lowest price first
```

`bids[price].push_back(order);` creates the level if it doesn't exist yet. To print, loop over a map with `for (const auto& [price, queue] : asks)`, then loop over the queue inside.

</details>

---

## Step 3: Match an incoming BUY (90 min)

**Goal:** an incoming BUY trades against the asks, following matching rules 2 and 4–8 in the spec.

**Learn:** a `while` loop with several stop conditions, `map.begin()`, `deque.front()`, `pop_front()`, `map.erase()`, `std::min`.

**Do:** while the BUY has quantity left, **and** there is at least one ask, **and** the best ask's price is at or below the BUY's price:

1. Trade with the first order in the best ask level, for the smaller of the two quantities.
2. Print the TRADE line and reduce both quantities.
3. If the resting order reached 0, remove it. If its level is now empty, remove the level.

After the loop, if the BUY still has quantity left, it rests in the bids.

Build it in this order, testing after each piece: a full fill (3a), a partial fill (3b), several levels with the price limit (3c), and first in, first out at one price (3d).

**Done when:** all four examples print exactly the expected output. Work each one out on paper first.

**Example 3a**: full fill

```
SELL 1 10 10000
BUY 2 10 10000
PRINT
```

<details><summary>Expected output</summary>

```
[2] TRADE buy=2 sell=1 qty=10 price=10000
[3] BOOK
  (empty)
```

</details>

**Example 3b**: partial fill; the rest of the BUY joins the book

```
SELL 1 4 10000
BUY 2 10 10000
PRINT
```

<details><summary>Expected output</summary>

```
[2] TRADE buy=2 sell=1 qty=4 price=10000
[3] BOOK
  BID 10000: 2x6
```

</details>

**Example 3c**: several levels; trades at the resting price; stops at the price limit

```
SELL 1 5 10000
SELL 2 5 10100
SELL 3 5 10200
BUY 4 12 10100
PRINT
```

<details><summary>Expected output</summary>

```
[4] TRADE buy=4 sell=1 qty=5 price=10000
[4] TRADE buy=4 sell=2 qty=5 price=10100
[5] BOOK
  ASK 10200: 3x5
  BID 10100: 4x2
```

The first trade is at 10000, not 10100: the resting order's price wins (rule 6).

</details>

**Example 3d**: first in, first out at one price

```
SELL 1 5 10000
SELL 2 5 10000
BUY 3 7 10000
PRINT
```

<details><summary>Expected output</summary>

```
[3] TRADE buy=3 sell=1 qty=5 price=10000
[3] TRADE buy=3 sell=2 qty=2 price=10000
[4] BOOK
  ASK 10000: 2x3
```

</details>

**Save each example as a test.** Put the commands in `tests/buy_full_fill.in.txt` and the expected output in `tests/buy_full_fill.expected.txt`, and so on for the others. Then `python run_tests.py` checks every test at once. The two cancel tests that came with the project will fail until Step 5; that's expected.

<details><summary>Hint</summary>

`asks.begin()` is the best ask level. Inside the loop: `auto level = asks.begin();`, `auto& queue = level->second;`, `Order& resting = queue.front();`. The traded quantity is `std::min(remaining, resting.qty)`. If `resting.qty` becomes 0, call `queue.pop_front()`. If `queue.empty()`, call `asks.erase(level)`.

The classic bug is forgetting to remove a filled order or an empty level. The loop then never ends; the test runner stops it after 5 seconds and tells you.

</details>

---

## Step 4: Match an incoming SELL (45 min)

**Goal:** the mirror image of Step 3. An incoming SELL trades against the bids, best (highest) bid first, while the bid's price is at or above the SELL's price.

**Done when:** this example prints exactly the expected output, and all your Step 3 tests still pass. Save it as a test too.

```
BUY 1 5 10200
BUY 2 5 10100
BUY 3 5 10000
SELL 4 12 10100
PRINT
```

<details><summary>Expected output</summary>

```
[4] TRADE buy=1 sell=4 qty=5 price=10200
[4] TRADE buy=2 sell=4 qty=5 price=10100
[5] BOOK
  ASK 10100: 4x2
  BID 10000: 3x5
```

</details>

**Bonus:** Steps 3 and 4 are almost the same code. Once both work, try writing the loop once, as a function both sides can use, and check the tests still pass.

---

## Step 5: CANCEL (60 min)

**Goal:** CANCEL removes a resting order; otherwise it is rejected (rule 9).

**Learn:** `std::unordered_map` for fast lookup by id, and why you must never loop over one (rule 5 in `CLAUDE.md`).

**Do:**

- Keep an `unordered_map` from order id to where that order rests: its side and its price.
- When an order starts resting, add it to the map. When it is fully filled or cancelled, remove it.
- CANCEL looks up the id. If found: remove the order from its queue (the others keep their places), remove the level if it's now empty, and print `CANCELLED`. If not found: print `CANCEL_REJECTED`.

**Done when:** `python run_tests.py` passes the two tests that came with the project, `cancel_then_buy` and `buy_then_cancel` (Rob's two cases, explained at the end of `docs/SPEC.md`), plus these three. Save them as tests.

**Example 5a**: unknown id, and cancelling twice

```
BUY 1 5 9900
CANCEL 7
CANCEL 1
CANCEL 1
PRINT
```

<details><summary>Expected output</summary>

```
[2] CANCEL_REJECTED id=7
[3] CANCELLED id=1
[4] CANCEL_REJECTED id=1
[5] BOOK
  (empty)
```

</details>

**Example 5b**: cancelling from the middle of a queue; the others keep their places

```
SELL 1 5 10000
SELL 2 5 10000
SELL 3 5 10000
CANCEL 2
BUY 4 7 10000
PRINT
```

<details><summary>Expected output</summary>

```
[4] CANCELLED id=2
[5] TRADE buy=4 sell=1 qty=5 price=10000
[5] TRADE buy=4 sell=3 qty=2 price=10000
[6] BOOK
  ASK 10000: 3x3
```

</details>

**Example 5c**: cancelling what's left of a partly filled order

```
SELL 1 10 10000
BUY 2 4 10000
CANCEL 1
PRINT
```

<details><summary>Expected output</summary>

```
[2] TRADE buy=2 sell=1 qty=4 price=10000
[3] CANCELLED id=1
[4] BOOK
  (empty)
```

</details>

<details><summary>Hint</summary>

```cpp
enum class Side { Buy, Sell };
struct Location { Side side; std::int64_t price; };
std::unordered_map<std::uint64_t, Location> resting;  // order id -> where it rests
```

To remove order `id` from a queue, search the queue for it (with `std::find_if`, or a plain loop) and call `queue.erase(it)`. This search is slow for very long queues; making it fast is a v2 job.

</details>

---

## Step 6: A test for every rule (45 min)

**Goal:** every rule in `docs/SPEC.md` has at least one test, and every test passes.

- Go through the matching rules one at a time. For each one, find a test that checks it. If there isn't one, write it.
- `run_tests.py` already runs every test twice and fails if the two outputs differ, so determinism is checked on every run.

**Done when:** `python run_tests.py` shows every test passing.

---

## Step 7: Git and GitHub (45 min)

**Goal:** v0 is public on GitHub with a clear README.

- Your home folder (`C:\Users\gehlo`) seems to have a `.git` folder in it, which makes Git treat your whole user folder as one big repository. Running `git init` **inside** `matching-engine` gives this project its own repository, which is what you want.
- Run `git init`, then `git add .`, then `git commit -m "v0: matching engine with price-time priority"`.
- Create a public repository on GitHub and push to it.
- Update the status line in `README.md`.

**Done when:** the repository is public, and its README explains what the engine does and how to run the tests.

---

## v1 checklist: correctness

The rules for every v1 step are in `docs/SPEC.md`.

- [x] Step 8: Split the code into `src/order_book.hpp` and `src/order_book.cpp`, with no change to any output (the 18 v0 tests prove it)
- [x] Step 9: Reject bad input: `REJECTED reason=...` for unknown commands, bad format, bad id, quantity or price, and duplicate ids. Lines with only spaces count as blank.
- [x] Step 10: Market orders: `MARKET_BUY` and `MARKET_SELL` trade at any price; any rest is dropped with `UNFILLED`
- [x] Step 11: `MODIFY`: same price and no more quantity keeps the order's place; anything else re-enters it at the back
- [x] Step 12: `tools/gen_commands.py`, a seeded random command generator, and a stress check in `run_tests.py`: 100,000 commands, run twice, every line well formed, every book sorted and never crossed

Things to be able to explain about v1:

- Why does a rejected command still use up a sequence number, but a rejected order not use up its id?
- Why can't `std::istringstream >> id` be trusted for `-1` or `5x`, and what does `std::from_chars` do differently?
- Why does increasing an order's quantity lose its place, but reducing it doesn't? (Think about fairness to the orders behind it.)
- Why is `match()` a template, and why can its definition live in `order_book.cpp` and not in the header?
- What can the stress check catch, and what can't it? (It checks the shape of the output, not that each trade is the right one.)

## v2 checklist: speed

Speed work changes how the book is stored, not what it prints. So the safety net comes first: every v2 step must keep `python run_tests.py` passing, including the differential test.

- [x] Step 13: Differential test. `tools/reference_engine.py` is a second implementation of `docs/SPEC.md` in Python, built differently on purpose. `run_tests.py` checks it against every expected output, then checks that the engine and the reference print exactly the same lines on four seeded stress files.
- [x] Step 14: Benchmarks. A separate program, `bench/bench.cpp`, that loads a command file into memory, runs it through `OrderBook` with output thrown away, and reports commands per second and latency per command (median, 99th percentile, worst). The clock lives only in the benchmark, never in the engine (rule 6). Record the v1 numbers in `docs/BENCHMARKS.md` before changing anything.
- [ ] Step 15: O(1) cancel and modify. Today CANCEL and MODIFY search their level one order at a time. Store orders in a pool (a `std::vector<Order>` plus a list of free slots), link the orders in each level as a doubly linked list of slot numbers, and have the index map an id straight to its slot. Measure again.
- [ ] Step 16: Faster input and output. Step 14's numbers say they matter: parsing takes longer than the book, and the whole engine takes about 5 times as long as the book alone (see `docs/BENCHMARKS.md`). Ideas: avoid building a `std::istringstream` for every line, and write output to one big buffer. Measure again.
- [ ] Step 17: README section with before/after numbers, and what each change bought.

Things to be able to explain about Step 14:

- Why does the benchmark parse the whole file before starting the clock?
- Why does it throw the output away, and why through a stream buffer that still counts bytes, instead of not printing at all?
- The clock moves in 100 ns steps here. What does that mean for a latency reported as "100 ns"?
- Why report p99 and p99.9 and not just the average?
- The book handles 2.3 million commands per second, but `engine.exe` only about 0.5 million. Where does the rest of the time go?

Things to be able to explain about Step 13:

- Why is a second implementation in a different style better than a second copy of the same code?
- The first version of the random generator missed a planted bug (a MODIFY with the same quantity losing its place). Why, and what was changed so it doesn't?
- A differential test says the two implementations agree. When could they agree and both be wrong?

## After v2 (planned in detail later)

- **v3, polish:** order expiry as a command (never by reading the clock), a test that catches hidden dependence on hash-map order, and a README that explains the design decisions.
