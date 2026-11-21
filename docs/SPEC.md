# Spec: the contract

This file defines exactly what the engine must do. The tests in `tests/` check it. Every later, faster version must print exactly the same output as the simple version for the same input.

## Input: the command file

- One command per line. Words are separated by spaces or tabs.
- A line that is empty, has only spaces or tabs, or whose first non-space character is `#` is ignored. It gets no sequence number.
- Every other line is a command, **even if it turns out to be invalid**. The first command gets sequence number 1, the next gets 2, and so on. An invalid command uses up its number and prints a rejection (see below).

| Command | Meaning |
|---|---|
| `BUY <id> <qty> <price>` | Limit buy: buy up to `qty`, paying at most `price`. Any rest joins the book. |
| `SELL <id> <qty> <price>` | Limit sell: sell up to `qty`, receiving at least `price`. Any rest joins the book. |
| `BUY <id> <qty> <price> GTT <time>` | The same, but any rest that joins the book leaves it when the clock reaches `time` (Good Till Time; rules 13–17) |
| `SELL <id> <qty> <price> GTT <time>` | The same, for a sell |
| `TIME <time>` | Move the engine's logical clock to `time`, and expire the orders whose time has come |
| `MARKET_BUY <id> <qty>` | Market buy: buy up to `qty` at any price. Any rest is dropped, never rests. |
| `MARKET_SELL <id> <qty>` | Market sell: sell up to `qty` at any price. Any rest is dropped, never rests. |
| `MODIFY <id> <qty> <price>` | Change resting order `id` to have `qty` left at `price` (rules 10–12) |
| `CANCEL <id>` | Remove resting order `id` from the book |
| `PRINT` | Print the current book |

- `id`: a positive whole number. Each new order (limit or market) must use an id that no earlier **accepted** order has used, even one that is already filled or cancelled.
- `qty`: a positive whole number.
- `price`: a positive whole number in **paise**. ₹100.00 is `10000`; ₹100.50 is `10050`.
- `time`: a whole number on the engine's logical clock, for example milliseconds since the market opened. The engine never reads a real clock (rule 6 in `CLAUDE.md`); time only moves when a `TIME` command says so.
- `GTT` is a keyword, upper case, and must be followed by exactly one number. Market orders and `MODIFY` don't take it.
- A number is written as digits, optionally with a leading `-`. Anything else (`10.50`, `+5`, `5x`) is not a number. Numbers must fit in a signed 64-bit integer.
- Command words are upper case. `buy` is an unknown command.

## Rejecting bad input

A command that breaks a rule prints exactly one line and changes nothing:

```
[seq] REJECTED reason=<reason>
```

The checks run in this order, and the first one that fails gives the reason:

| Order | Reason | When |
|---|---|---|
| 1 | `unknown_command` | The first word is not one of the commands above |
| 2 | `bad_format` | Wrong number of words after the command, one of them is not a number, or the word before a GTT time is not exactly `GTT` |
| 3 | `bad_id` | `id` is 0 or negative |
| 4 | `bad_quantity` | `qty` is 0 or negative (BUY, SELL, MARKET_*, MODIFY) |
| 5 | `bad_price` | `price` is 0 or negative (BUY, SELL, MODIFY) |
| 6 | `bad_expiry` | A GTT time is not later than the clock's current time |
| 7 | `duplicate_id` | A new order (BUY, SELL, MARKET_*) reuses an id an earlier accepted order used |
| 8 | `time_backwards` | A `TIME` command's time is earlier than the clock's current time |

A rejected order does not use up its id: a later valid order may use it.

A well-formed `CANCEL` or `MODIFY` for an id that is not resting is not a `REJECTED` line; it prints `CANCEL_REJECTED` or `MODIFY_REJECTED` (rules 9 and 10).

## The book

- **Bids** are resting buy orders. **Asks** are resting sell orders.
- Orders are grouped into **price levels**. Within a level, orders wait in a queue in arrival order: first in, first out.
- The **best bid** is the highest bid price. The **best ask** is the lowest ask price.
- After every command, the best bid is below the best ask (the book is never "crossed").

## Matching rules

1. Each command is processed completely before the next one starts.
2. An incoming **BUY** at price `P` trades with asks priced **at or below** `P`, best (lowest) ask first. A **MARKET_BUY** trades with asks at any price, best first.
3. An incoming **SELL** at price `P` trades with bids priced **at or above** `P`, best (highest) bid first. A **MARKET_SELL** trades with bids at any price, best first.
4. At the same price, the order that arrived first (lower sequence number) trades first.
5. Each trade is for the smaller of the two remaining quantities.
6. **Trade price = the resting order's price**, not the incoming order's price.
7. A resting order whose quantity reaches 0 leaves the book. A price level with no orders left is removed.
8. If an incoming limit order still has quantity left when nothing more can trade, the rest joins the book at its own price, at the back of that level's queue. If an incoming **market** order still has quantity left, the rest is dropped and `UNFILLED` is printed.
9. `CANCEL` removes a resting order, even one that was partly filled. If the id is not resting in the book (it never existed, was fully filled, was a market order, or was already cancelled), the cancel is rejected and nothing changes.
10. `MODIFY` changes a resting order. `qty` is the new quantity **left**, not the original size. If the id is not resting, it prints `MODIFY_REJECTED` and nothing changes.
11. If the price stays the same and the quantity does not go up, the order keeps its place in the queue and only its quantity changes.
12. Otherwise (a new price, or more quantity) the order loses its place: it is taken out of the book and enters again, with the same id, as if it were a new limit order arriving now. It gets this command's sequence number, may trade straight away (rules 2–8), and any rest goes to the back of its level.

## Time and expiry

13. The engine has a logical clock. It starts at 0 and only moves when a `TIME <t>` command moves it. `TIME` with the clock's current time is allowed and changes nothing; an earlier time is rejected with `time_backwards`.
14. A GTT order's time must be later than the clock's current time, or it is rejected with `bad_expiry`. It then trades exactly like any other limit order; only the rest that joins the book can expire. An order with no GTT never expires.
15. When `TIME t` moves the clock, every resting order whose GTT time is at or before `t` leaves the book, and `EXPIRED` is printed for each. They go in order of GTT time, earliest first; orders with the same time go in the order they entered the book (lower sequence number first; for an order a MODIFY moved, the MODIFY's sequence number). A price level left empty is removed.
16. An order that is fully filled or cancelled before its time never expires. One that is partly filled expires with the quantity it has left.
17. `MODIFY` keeps an order's GTT time, whether the order keeps its place or enters again.

## Output

Every output line starts with `[seq]`: the sequence number of the command that caused it.

| Event | Output line |
|---|---|
| A trade | `[seq] TRADE buy=<buy id> sell=<sell id> qty=<qty> price=<price>` |
| Market order rest dropped | `[seq] UNFILLED id=<id> qty=<qty dropped>` |
| A resting order expired | `[seq] EXPIRED id=<id> qty=<qty it had left>` |
| Modify accepted | `[seq] MODIFIED id=<id> qty=<new qty> price=<new price>` |
| Modify of an id not resting | `[seq] MODIFY_REJECTED id=<id>` |
| Cancel succeeded | `[seq] CANCELLED id=<id>` |
| Cancel of an id not resting | `[seq] CANCEL_REJECTED id=<id>` |
| Invalid command | `[seq] REJECTED reason=<reason>` |
| `PRINT` | `[seq] BOOK`, then one line per price level (see below) |

An order that rests without trading prints nothing, and so does a `TIME` that expires nothing. For a `MODIFY`, the `MODIFIED` line comes first, then any trades it causes. For a market order, the trades come first, then `UNFILLED` if anything is left.

`PRINT` prints `[seq] BOOK`, then all ask levels, **lowest price first**, then all bid levels, **highest price first**. Each level is one line that starts with two spaces. Within a level, orders are listed in queue order as `<id>x<qty>`; for example, `2x7` is order 2 with 7 left.

```
[6] BOOK
  ASK 10000: 5x3
  ASK 10100: 4x10
  BID 9900: 1x5 2x7
  BID 9800: 3x4
```

If the book is empty, the only line after `[seq] BOOK` is `  (empty)`:

```
[3] BOOK
  (empty)
```

## The two cases behind this contract

Orders 1 and 2 each rest as a sell of 10 at ₹100 (`10000`), and order 1 is first in line. The same two commands in a different order must give different results.

**Cancel, then buy:** order 1 is cancelled, so the buy fills against order 2.

```
SELL 1 10 10000
SELL 2 10 10000
CANCEL 1
BUY 3 10 10000
PRINT
```

```
[3] CANCELLED id=1
[4] TRADE buy=3 sell=2 qty=10 price=10000
[5] BOOK
  (empty)
```

**Buy, then cancel:** the buy fills against order 1, so the cancel is rejected and order 2 stays in the book.

```
SELL 1 10 10000
SELL 2 10 10000
BUY 3 10 10000
CANCEL 1
PRINT
```

```
[3] TRADE buy=3 sell=1 qty=10 price=10000
[4] CANCEL_REJECTED id=1
[5] BOOK
  ASK 10000: 2x10
```

Because every command has a sequence number, every result can be explained ("order 3 filled against order 1 because order 1 was first in line at 10000") and reproduced by replaying the same file.
