"""Generate a random but repeatable command file for stress tests.

The randomness lives here, outside the engine, with a fixed seed: the same seed and count
always give exactly the same file (CLAUDE.md rule 6).

Usage, from the matching-engine folder:
    python tools/gen_commands.py --seed 1 --count 100000 --out build/stress.txt
"""

import argparse
import random

# A few invalid lines, so the stress test also exercises every rejection path.
BAD_LINES = [
    "BUY 5",
    "SELL x 5 10000",
    "BUY 1 5 100.50",
    "HELLO",
    "MODIFY 1 5",
    "CANCEL",
    "MARKET_BUY 0 5",
    "BUY 1 0 10000",
    "SELL 1 5 -1",
]


# Invalid lines about time and expiry, used only when with_expiry is on.
BAD_EXPIRY_LINES = [
    "TIME",
    "TIME x",
    "BUY 1 5 10000 GTT",
    "BUY 1 5 10000 gtt 5",
    "SELL 1 5 10000 GTT 5 6",
    "MARKET_BUY 1 5 GTT 5",
]


def generate(seed, count, print_every=10_000, with_expiry=True):
    """Return `count` command lines, plus a final PRINT.

    with_expiry adds TIME commands and GTT orders. With it off, the random numbers are drawn in
    exactly the same order as before expiry existed, so the same seed gives the same file as in
    v2. The benchmark workload relies on that, to stay comparable with older results.
    """
    rng = random.Random(seed)
    lines = []
    next_id = 1
    maybe_resting = []  # ids of limit orders that might still rest (the engine decides)
    sent = {}  # id -> (qty, price) last sent for that order, to aim MODIFY at the rule 11/12 edges
    now = 0  # the logical clock, as the engine will see it

    for i in range(1, count + 1):
        if i % print_every == 0:
            lines.append("PRINT")
            continue

        if with_expiry and rng.random() < 0.02:
            t = rng.random()
            if t < 0.85:
                now += rng.randint(1, 60)
                lines.append(f"TIME {now}")
            elif t < 0.90:
                lines.append(f"TIME {now}")  # the same time again: allowed, changes nothing
            elif t < 0.95:
                lines.append(f"TIME {now - rng.randint(1, 10)}")  # must be time_backwards
            else:
                lines.append(rng.choice(BAD_EXPIRY_LINES))
            continue

        r = rng.random()
        if r < 0.50:
            # Limit order. Buy and sell prices overlap, so many orders trade.
            side = rng.choice(("BUY", "SELL"))
            qty = rng.randint(1, 100)
            if side == "BUY":
                price = rng.randint(9_980, 10_010)
            else:
                price = rng.randint(9_990, 10_020)
            gtt = ""
            if with_expiry and rng.random() < 0.3:
                if rng.random() < 0.05:
                    gtt = f" GTT {now - rng.randint(0, 5)}"  # not in the future: bad_expiry
                else:
                    gtt = f" GTT {now + rng.randint(1, 400)}"
            lines.append(f"{side} {next_id} {qty} {price}{gtt}")
            maybe_resting.append(next_id)
            sent[next_id] = (qty, price)
            next_id += 1
        elif r < 0.70:
            # Cancel, usually an order that might be resting, sometimes an unknown id.
            if maybe_resting and rng.random() < 0.9:
                k = rng.randrange(len(maybe_resting))
                order_id = maybe_resting[k]
                maybe_resting[k] = maybe_resting[-1]  # remove it without shifting the list
                maybe_resting.pop()
            else:
                order_id = rng.randint(1, next_id + 10)
            lines.append(f"CANCEL {order_id}")
        elif r < 0.82:
            # Modify an order. Most modifies keep the price and send the same, less or more
            # quantity than last time, because rules 11 and 12 change exactly at those edges.
            # (If the order was partly filled, "the same as last time" is really more.)
            order_id = rng.choice(maybe_resting) if maybe_resting else rng.randint(1, next_id)
            old_qty, old_price = sent.get(order_id, (rng.randint(1, 100), 10_000))
            kind = rng.random()
            if kind < 0.25:
                qty, price = old_qty, old_price                       # same price, same qty
            elif kind < 0.50:
                qty, price = rng.randint(1, old_qty), old_price       # same price, not more
            elif kind < 0.65:
                qty, price = old_qty + rng.randint(1, 20), old_price  # same price, more
            else:
                qty, price = rng.randint(1, 100), rng.randint(9_985, 10_015)  # anything
            lines.append(f"MODIFY {order_id} {qty} {price}")
            sent[order_id] = (qty, price)
        elif r < 0.92:
            side = rng.choice(("MARKET_BUY", "MARKET_SELL"))
            lines.append(f"{side} {next_id} {rng.randint(1, 150)}")
            next_id += 1
        elif r < 0.97:
            # Reuse an old id on purpose: must be rejected as duplicate_id.
            if next_id > 1:
                old = rng.randint(1, next_id - 1)
                lines.append(f"BUY {old} 1 10000")
            else:
                lines.append("PRINT")
        else:
            lines.append(rng.choice(BAD_LINES))

    lines.append("PRINT")
    return lines


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--count", type=int, default=100_000)
    parser.add_argument("--out", required=True, help="file to write")
    args = parser.parse_args()

    lines = generate(args.seed, args.count)
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {len(lines)} commands to {args.out}")


if __name__ == "__main__":
    main()
