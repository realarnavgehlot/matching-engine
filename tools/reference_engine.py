"""A second, independent implementation of docs/SPEC.md, written to be obviously right, not fast.

run_tests.py runs it on the same commands as the C++ engine and checks that both print exactly
the same lines. This is a *differential test*: if the two disagree anywhere, one of them is wrong.

It is built differently from the C++ engine on purpose, so the same bug is unlikely in both:
- Queue order is never stored. Each resting order has a `priority`: the sequence number of the
  command that last put it in the book. The front of a level is the order with the lowest priority.
- A price level is a plain set of ids. The best price is found with min() or max() every time.
- A market order has no limit at all (None), not a very large or very small number.
- Expiry keeps no sorted list: on each TIME it scans every resting order and sorts the ones due.

Usage, from the matching-engine folder:
    python tools/reference_engine.py data/sample.txt
"""

import re
import sys

INT64_MIN = -(2**63)
INT64_MAX = 2**63 - 1
NUMBER = re.compile(r"-?[0-9]+")  # digits, optionally a leading '-' (docs/SPEC.md, Input)

# How many numbers follow each command word.
NUMBER_COUNT = {
    "BUY": 3,
    "SELL": 3,
    "MARKET_BUY": 2,
    "MARKET_SELL": 2,
    "MODIFY": 3,
    "CANCEL": 1,
    "PRINT": 0,
    "TIME": 1,
}


class Order:
    __slots__ = ("id", "side", "price", "qty", "priority", "expiry")

    def __init__(self, order_id, side, price, qty, priority, expiry):
        self.id = order_id
        self.side = side  # "BUY" or "SELL"
        self.price = price
        self.qty = qty
        self.priority = priority
        self.expiry = expiry  # GTT time, or None


class ReferenceBook:
    def __init__(self):
        self.out = []
        self.resting = {}  # id -> Order, for every order in the book
        self.levels = {"BUY": {}, "SELL": {}}  # side -> price -> set of ids at that price
        self.used_ids = set()  # every id an accepted order has used
        self.now = 0  # the logical clock

    # --- output ---------------------------------------------------------

    def emit(self, seq, text):
        self.out.append(f"[{seq}] {text}")

    def reject(self, seq, reason):
        self.emit(seq, f"REJECTED reason={reason}")

    # --- the book -------------------------------------------------------

    def best_price(self, side):
        prices = self.levels[side]
        if not prices:
            return None
        return max(prices) if side == "BUY" else min(prices)

    def front_order(self, side, price):
        ids = self.levels[side][price]
        return min((self.resting[i] for i in ids), key=lambda order: order.priority)

    def add(self, order):
        self.resting[order.id] = order
        self.levels[order.side].setdefault(order.price, set()).add(order.id)

    def remove(self, order):
        del self.resting[order.id]
        ids = self.levels[order.side][order.price]
        ids.remove(order.id)
        if not ids:
            del self.levels[order.side][order.price]

    def match(self, seq, side, order_id, qty, limit):
        """Trade against the other side. `limit` is None for a market order. Returns qty left."""
        other = "SELL" if side == "BUY" else "BUY"
        while qty > 0:
            best = self.best_price(other)
            if best is None:
                break
            if limit is not None:
                if side == "BUY" and best > limit:
                    break
                if side == "SELL" and best < limit:
                    break
            resting = self.front_order(other, best)
            traded = min(qty, resting.qty)
            if side == "BUY":
                buy_id, sell_id = order_id, resting.id
            else:
                buy_id, sell_id = resting.id, order_id
            self.emit(seq, f"TRADE buy={buy_id} sell={sell_id} qty={traded} price={best}")
            qty -= traded
            resting.qty -= traded
            if resting.qty == 0:
                self.remove(resting)
        return qty

    def enter(self, seq, side, order_id, qty, price, expiry):
        left = self.match(seq, side, order_id, qty, price)
        if left > 0:
            self.add(Order(order_id, side, price, left, priority=seq, expiry=expiry))

    # --- commands -------------------------------------------------------

    def limit(self, seq, side, order_id, qty, price, expiry):
        if order_id <= 0:
            return self.reject(seq, "bad_id")
        if qty <= 0:
            return self.reject(seq, "bad_quantity")
        if price <= 0:
            return self.reject(seq, "bad_price")
        if expiry is not None and expiry <= self.now:
            return self.reject(seq, "bad_expiry")
        if order_id in self.used_ids:
            return self.reject(seq, "duplicate_id")
        self.used_ids.add(order_id)
        self.enter(seq, side, order_id, qty, price, expiry)

    def set_time(self, seq, time):
        if time < self.now:
            return self.reject(seq, "time_backwards")
        self.now = time
        due = [o for o in self.resting.values() if o.expiry is not None and o.expiry <= time]
        due.sort(key=lambda order: (order.expiry, order.priority))
        for order in due:
            self.remove(order)
            self.emit(seq, f"EXPIRED id={order.id} qty={order.qty}")

    def market(self, seq, side, order_id, qty):
        if order_id <= 0:
            return self.reject(seq, "bad_id")
        if qty <= 0:
            return self.reject(seq, "bad_quantity")
        if order_id in self.used_ids:
            return self.reject(seq, "duplicate_id")
        self.used_ids.add(order_id)
        left = self.match(seq, side, order_id, qty, limit=None)
        if left > 0:
            self.emit(seq, f"UNFILLED id={order_id} qty={left}")

    def modify(self, seq, order_id, qty, price):
        if order_id <= 0:
            return self.reject(seq, "bad_id")
        if qty <= 0:
            return self.reject(seq, "bad_quantity")
        if price <= 0:
            return self.reject(seq, "bad_price")
        order = self.resting.get(order_id)
        if order is None:
            return self.emit(seq, f"MODIFY_REJECTED id={order_id}")
        self.emit(seq, f"MODIFIED id={order_id} qty={qty} price={price}")
        if price == order.price and qty <= order.qty:
            order.qty = qty  # rule 11: keeps its priority
            return
        self.remove(order)  # rule 12: enters again with this command's priority,
        self.enter(seq, order.side, order_id, qty, price, order.expiry)  # rule 17: same expiry

    def cancel(self, seq, order_id):
        if order_id <= 0:
            return self.reject(seq, "bad_id")
        order = self.resting.get(order_id)
        if order is None:
            return self.emit(seq, f"CANCEL_REJECTED id={order_id}")
        self.remove(order)
        self.emit(seq, f"CANCELLED id={order_id}")

    def print_book(self, seq):
        self.emit(seq, "BOOK")
        if not self.resting:
            self.out.append("  (empty)")
            return
        for side, name, descending in (("SELL", "ASK", False), ("BUY", "BID", True)):
            for price in sorted(self.levels[side], reverse=descending):
                orders = sorted(
                    (self.resting[i] for i in self.levels[side][price]),
                    key=lambda order: order.priority,
                )
                entries = " ".join(f"{o.id}x{o.qty}" for o in orders)
                self.out.append(f"  {name} {price}: {entries}")


def parse_numbers(words, count):
    """The words as integers, or None if there are not exactly `count` valid numbers."""
    if len(words) != count:
        return None
    values = []
    for word in words:
        if not NUMBER.fullmatch(word):
            return None
        value = int(word)
        if not INT64_MIN <= value <= INT64_MAX:
            return None
        values.append(value)
    return values


def run(lines):
    """Run every command line and return the output lines."""
    book = ReferenceBook()
    seq = 0
    for line in lines:
        text = line.strip(" \t\r\n")
        if not text or text.startswith("#"):
            continue
        seq += 1
        words = text.split()
        cmd = words[0]
        if cmd not in NUMBER_COUNT:
            book.reject(seq, "unknown_command")
            continue
        args, expiry = words[1:], None
        if cmd in ("BUY", "SELL") and len(args) == 5 and args[3] == "GTT":
            gtt = parse_numbers(args[4:], 1)
            if gtt is None:
                book.reject(seq, "bad_format")
                continue
            args, expiry = args[:3], gtt[0]
        n = parse_numbers(args, NUMBER_COUNT[cmd])
        if n is None:
            book.reject(seq, "bad_format")
        elif cmd in ("BUY", "SELL"):
            book.limit(seq, cmd, n[0], n[1], n[2], expiry)
        elif cmd == "TIME":
            book.set_time(seq, n[0])
        elif cmd in ("MARKET_BUY", "MARKET_SELL"):
            book.market(seq, cmd[len("MARKET_"):], n[0], n[1])
        elif cmd == "MODIFY":
            book.modify(seq, n[0], n[1], n[2])
        elif cmd == "CANCEL":
            book.cancel(seq, n[0])
        else:
            book.print_book(seq)
    return book.out


def run_file(path):
    with open(path, encoding="utf-8", errors="surrogateescape") as f:
        return run(f)


def main():
    if len(sys.argv) < 2:
        print("usage: python tools/reference_engine.py <command file>", file=sys.stderr)
        sys.exit(1)
    for line in run_file(sys.argv[1]):
        print(line)


if __name__ == "__main__":
    main()
