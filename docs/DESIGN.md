# Design decisions

Why the engine is built the way it is. For each decision: what was chosen, why, what was rejected, and what it costs. The rules themselves are in [`SPEC.md`](SPEC.md); the measurements behind the speed decisions are in [`BENCHMARKS.md`](BENCHMARKS.md).

## What the design is for

In order of priority:

1. **Correct:** every trade follows price-time priority exactly.
2. **Deterministic:** the same command file always gives byte-for-byte the same output, on any machine, so any result can be replayed and explained.
3. **Explainable:** every output line can be traced to the command that caused it.
4. **Fast:** but never at the cost of 1–3.

---

## Numbers and time

### Prices are whole numbers of paise, in `std::int64_t`

**Why:** price levels are found by exact equality ("is there already a level at 10050?"). Floating-point numbers can't represent most decimal prices exactly: in `double`, `0.1 + 0.2 != 0.3`. Two orders meant for the same price could land on different levels, and a trade could be off by a fraction of a paisa. Whole paise are exact, and 64 bits hold any price that will ever exist.

**Rejected:** `double` (inexact); a decimal library (exact, but slower and an extra dependency for no gain, since every price is a whole number of paise anyway).

### Time is the sequence number; the clock is logical

Every command gets the next sequence number, and "earlier" means "lower sequence number". For expiry, the engine has a logical clock that only moves when a `TIME <t>` command says so.

**Why:** reading the computer's clock would make the output depend on when the program ran and how fast the machine is. The same file could expire different orders on different runs, so results could no longer be replayed or tested. Real exchanges do the same in their matching core: timestamps come from the input (the gateway stamps each message), not from the matcher reading a clock.

**Rejected:** `std::chrono` inside the engine. The only clock in the project is in `bench/bench.cpp`, which measures the engine from outside.

---

## Threads

### One thread owns the book; no locks

**Why:** matching is a strict sequence. Each command must see the book exactly as the previous one left it, so there's nothing useful to do in parallel inside one book. Locks would add cost and, worse, make the order of events depend on thread timing, which breaks determinism. Real exchanges scale by giving each instrument (each stock) its own single-threaded matcher, not by locking one book.

**Cost:** one book can't use more than one CPU core. For several instruments, the natural next step is one engine per instrument.

---

## The book

### Price levels in `std::map`; bids with `std::greater`

**Why:** a `std::map` keeps its keys sorted, so the best price is always `begin()`: the lowest ask, and, with `std::greater`, the highest bid. Matching only ever touches the front, and PRINT walks the levels in order for free. Inserting or removing a level is O(log L), where L is the number of price levels, which stays small (about 30–40 in the benchmark).

**Rejected:**
- *A sorted `std::vector` of levels:* faster to walk, but inserting a new level in the middle shifts every level after it.
- *An array indexed by price* (one slot per possible paisa): O(1) for everything, and what the fastest engines use, but it needs known price bounds and tick sizes, and it wastes memory on prices nobody uses.
- *A heap (`std::priority_queue`):* gives the best price fast, but can't remove a level in the middle or list the levels in order.

### Templates so bids and asks share code

The bid map and the ask map have different types (different comparators), so `match`, `rest`, `unlink` and `printSide` are templates over the map type.

**Why:** the bid side and ask side are mirror images. One template means one copy of the matching loop, so a fix can't be made on one side and forgotten on the other.

**Rejected:** two copies of each function (easy to let drift apart); a virtual base class (extra indirection on every call, for no benefit).

### Orders in a pool, each level a doubly linked list of slots

*(v2, Step 15. v1 used a `std::deque<Order>` per level.)*

Every resting order lives in one `std::vector<Order>`, the **pool**, and is named by its position in it, its **slot**. Each order stores the slots of the orders in front of and behind it at the same price; a level only stores its first and last slot. A hash table maps each id to its slot.

**Why:** with a deque, CANCEL and MODIFY had to search the level from the front to find the order, so their cost grew with the queue's length (about 5 µs in a 20,000-order queue). With the linked list, removing any order is a few slot updates, whatever the queue's length: about 0.2 µs.

**Why slots and not pointers:** a `std::vector` that grows moves all its elements to a new block of memory, which would leave every pointer and reference into it dangling. A slot number stays valid. (This is also why `rest()` calls `allocate()` before taking any `Order&`.)

**Why a free list:** slots of filled or cancelled orders are reused, so the pool's size follows the number of *resting* orders, not the total number ever sent.

**Rejected:** `std::list` per level (one memory allocation per order, and still a search unless you also store iterators); keeping the deque and storing positions (positions shift whenever an earlier order leaves).

**Cost:** a linked list jumps around memory, so the CPU's cache helps less. Printing a large book got slower (about 4 ms → 12 ms). PRINT is rare, so this is a good trade.

### Hash tables only for lookups, never for order

`index_` (id → slot) and `usedIds_` (every id ever accepted) are `std::unordered_map`/`std::unordered_set`: O(1) lookups by id.

**Why never loop over them:** the order in which a hash table visits its entries depends on the hash function, the number of buckets, the order of inserts and the library version. If any output depended on that order, the same input could give different output on another compiler or after the table grew. This is rule 5 in `CLAUDE.md`, and Step 19 tests it: `run_tests.py` builds the engine a second time with a deliberately different hash function and requires identical output.

**Size hint:** a hash table that grows moves every entry into a bigger one. With hundreds of thousands of ids, one such step took over 10 ms. `OrderBook` takes an `expectedOrders` hint, and `main()` guesses it from the file's size, so the tables are sized once (Step 17).

**Cost:** `usedIds_` never shrinks. A real exchange would reset ids per trading day; here a file is one session.

### Expiry in a `std::map` keyed by (GTT time, sequence number)

*(v3, Step 18.)*

**Why:** the front of the map is always the next order to expire, and ties on GTT time are broken by sequence number, so the order of `EXPIRED` lines is fully defined. Walking a `std::map` is deterministic, because its order is set by its keys. When an order is filled, cancelled or moved by MODIFY, its entry is removed, so the map only ever holds resting orders.

**Rejected:** a `std::priority_queue` (it can't remove a cancelled order from the middle; "lazy deletion", skipping dead entries later, works but is easier to get wrong); scanning every order on each `TIME` (simple, and what the reference engine does, but O(book size) per `TIME`).

**Cost:** each order grew from 48 to 64 bytes (`std::optional<std::int64_t>` is 16 bytes), about 4% slower end to end even with no GTT orders.

---

## The rules

### MODIFY keeps the queue position only if the price stays and the quantity doesn't grow

**Why:** fairness to the orders behind it. Reducing size never hurts anyone who's waiting. Adding size, or moving to a new price, is like placing a new order, so it goes to the back. This is how most exchanges treat a modify.

### A market order never rests

What a market order can't fill is dropped and reported as `UNFILLED`. A market order with no price limit, left in the book, would match the next order at *any* price, however bad.

### IOC and FOK never rest; FOK checks before it trades

*(v4, Step 21.)* An IOC is a limit order whose rest is dropped instead of resting. A FOK must not trade at all unless it can fill completely, so it first checks, then trades: `canFill()` walks the orders it would trade with, best price first, and stops as soon as there is enough.

**Why walk instead of keeping a total per level:** a running total of quantity per level would make the check O(levels) instead of O(orders), but the total of many huge orders at one price could overflow `std::int64_t` (undefined behaviour in C++). Counting the needed quantity *down* can't overflow. FOK orders are a small share of traffic, so the simpler, safe check wins.

**One time-in-force per order:** `GTT`, `IOC` and `FOK` are alternatives, chosen with one `TimeInForce` enum, so impossible combinations ("IOC that expires at 50") can't even be written.

### Invalid input is rejected with a reason, in a fixed order of checks

**Why:** every line gets a sequence number and exactly one outcome, so the output stays explainable even for bad input. The checks run in a fixed order, so a line with several problems always gets the same reason. A rejected order doesn't use up its id, because it never existed.

**Why `std::from_chars`:** `std::istringstream >>` lets some bad input through: for example, reading `-1` into an unsigned number silently wraps around. `from_chars` either reads the whole word as a valid number or fails.

---

## Input and output

### A parser on `std::string_view`

*(v2, Step 16.)* Each line is split into views that point into the line, without copying, and each number goes straight to `std::from_chars`. The v1 parser built a `std::istringstream`, a `std::string` per word and a `std::vector` per line: about 9 times slower. The new parser was checked against the old one on 3,000,000 random lines before it replaced it.

### Our own output buffer

*(v2, Step 16.)* `Output` collects text in a 64 KB buffer, formats numbers with `std::to_chars`, and hands the stream one big block at a time. `std::ostream`'s `<<` formats every number through the locale machinery and passes each small piece on its own.

**Rejected:** `std::ios::sync_with_stdio(false)`, the usual advice. Measured, it made the engine slower on this machine: on Windows with this compiler it leaves `std::cout` with a 512-byte buffer.

---

## Testing

The engine is checked several different ways, because each one catches bugs the others miss:

| Check | Catches |
|---|---|
| Hand-written tests (`tests/`), each output worked out on paper | Misreading a rule in a small, understood case |
| Every test run twice | Non-determinism (for example, reading uninitialized memory) |
| A reference engine in Python (`tools/reference_engine.py`), built differently on purpose, compared line by line on hundreds of thousands of seeded random commands | Bugs that only show up in rare combinations |
| Shape checks on stress output (books sorted, never crossed, no order trading with itself) | Corrupt state, even where the reference agrees |
| A second build with a scrambled hash function | Hidden dependence on hash-table order |
| Planted bugs ("mutation testing") | Checks that can't fail |
| Byte-for-byte comparison of the full 41.7 MB benchmark output before and after each speed change | Any change in behaviour from an optimization |

The random generator lives outside the engine and always uses a fixed seed, so every random test is repeatable.

**Why the reference is built differently:** a second copy of the same code would repeat the same bugs. The reference stores no queue order (each order has a priority number), finds the best price with `min`/`max` every time, and finds expiring orders by scanning and sorting.

**Known gap:** if the spec itself is misread the same way in both engines, they agree and are both wrong. The hand-written tests, worked out on paper from the spec, are the defence against that.

---

## Benchmarks

- **The clock lives only in `bench/bench.cpp`.** The engine never times itself.
- **Parse first, then time the book**, so the book's time is the book's alone. End-to-end time is measured separately, from outside, by `tools/time_engine.py`.
- **Output is formatted, then thrown away** through a stream buffer that counts bytes, so the formatting work still happens but the disk isn't measured.
- **Alternate old and new versions** when comparing small differences: on a laptop, noise between rounds can be bigger than the effect.
- **Report the clock's resolution** (100 ns here), so small latencies aren't over-read.

---

## Known limits

- One instrument per engine; no network interface, no persistence or recovery.
- No traders or accounts, so no self-trade prevention.
- Slots are 32-bit: at most about 4 billion orders resting at once.
- `usedIds_` grows for the whole run.
- PRINT of a very large book is slow (milliseconds), because of linked-list memory access.
