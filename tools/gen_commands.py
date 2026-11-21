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


def generate(seed, count, print_every=10_000):
    """Return `count` command lines, plus a final PRINT."""
    rng = random.Random(seed)
    lines = []
    next_id = 1
    maybe_resting = []  # ids of limit orders that might still rest (the engine decides)

    for i in range(1, count + 1):
        if i % print_every == 0:
            lines.append("PRINT")
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
            lines.append(f"{side} {next_id} {qty} {price}")
            maybe_resting.append(next_id)
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
            # Modify a random order: sometimes keeping its price, sometimes not.
            order_id = rng.choice(maybe_resting) if maybe_resting else rng.randint(1, next_id)
            qty = rng.randint(1, 100)
            price = rng.randint(9_985, 10_015)
            lines.append(f"MODIFY {order_id} {qty} {price}")
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
