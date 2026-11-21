# Spec: the contract

This file defines exactly what the engine must do. The tests in `tests/` check it. Every later, faster version must print exactly the same output as the simple version for the same input.

## Input: the command file

- One command per line. Words are separated by spaces.
- Blank lines and lines starting with `#` are ignored. They get no sequence number.
- Every other line is a command. The first command gets sequence number 1, the next gets 2, and so on.

| Command | Meaning |
|---|---|
| `BUY <id> <qty> <price>` | Limit buy: buy up to `qty`, paying at most `price` |
| `SELL <id> <qty> <price>` | Limit sell: sell up to `qty`, receiving at least `price` |
| `CANCEL <id>` | Remove resting order `id` from the book |
| `PRINT` | Print the current book |

- `id`: a positive whole number. Each new order uses a new id.
- `qty`: a positive whole number.
- `price`: a whole number in **paise**. ₹100.00 is `10000`; ₹100.50 is `10050`.

In v0 the input is assumed to be valid. Handling bad input (duplicate ids, zero quantity, unknown commands) comes in v1.

## The book

- **Bids** are resting buy orders. **Asks** are resting sell orders.
- Orders are grouped into **price levels**. Within a level, orders wait in a queue in arrival order: first in, first out.
- The **best bid** is the highest bid price. The **best ask** is the lowest ask price.

## Matching rules

1. Each command is processed completely before the next one starts.
2. An incoming **BUY** at price `P` trades with asks priced **at or below** `P`, best (lowest) ask first.
3. An incoming **SELL** at price `P` trades with bids priced **at or above** `P`, best (highest) bid first.
4. At the same price, the order that arrived first (lower sequence number) trades first.
5. Each trade is for the smaller of the two remaining quantities.
6. **Trade price = the resting order's price**, not the incoming order's price.
7. A resting order whose quantity reaches 0 leaves the book. A price level with no orders left is removed.
8. If the incoming order still has quantity left when nothing more can trade, the rest joins the book at its own price, at the back of that level's queue.
9. `CANCEL` removes a resting order, even one that was partly filled. If the id is not resting in the book (it never existed, was fully filled, or was already cancelled), the cancel is rejected and nothing changes.

## Output

Every output line starts with `[seq]`: the sequence number of the command that caused it.

| Event | Output line |
|---|---|
| A trade | `[seq] TRADE buy=<buy id> sell=<sell id> qty=<qty> price=<price>` |
| Cancel succeeded | `[seq] CANCELLED id=<id>` |
| Cancel rejected | `[seq] CANCEL_REJECTED id=<id>` |
| `PRINT` | `[seq] BOOK`, then one line per price level (see below) |

An order that rests without trading prints nothing.

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
