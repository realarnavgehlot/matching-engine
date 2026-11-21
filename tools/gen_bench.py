"""Write the benchmark command files into build/. Every file comes from a fixed seed, so the
numbers in docs/BENCHMARKS.md can always be reproduced.

Usage, from the matching-engine folder:
    python tools/gen_bench.py
Then:
    .\\bench.exe build\\bench_mixed.txt
    .\\bench.exe build\\bench_deep_cancel.txt
    .\\bench.exe build\\bench_deep_modify.txt
"""

import os
import random
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
from gen_commands import generate  # noqa: E402
DEEP = 20_000  # orders resting at one price in the deep_* workloads


def mixed():
    """A realistic mix: the stress generator, 1,000,000 commands, one PRINT at the end.
    Without expiry, IOC or FOK, so the file is the same as in v2 and the numbers stay comparable."""
    return generate(seed=1, count=1_000_000, print_every=10**12, with_expiry=False,
                    with_ioc_fok=False)


def deep_cancel():
    """DEEP sells at one price, then every one cancelled in random order.
    Today each CANCEL searches the level from the front, so this grows like DEEP squared."""
    rng = random.Random(7)
    lines = [f"SELL {i} 10 10000" for i in range(1, DEEP + 1)]
    ids = list(range(1, DEEP + 1))
    rng.shuffle(ids)
    lines += [f"CANCEL {i}" for i in ids]
    lines.append("PRINT")
    return lines


def deep_modify():
    """DEEP sells at one price, then every one reduced (keeping its place) in random order."""
    rng = random.Random(8)
    lines = [f"SELL {i} 10 10000" for i in range(1, DEEP + 1)]
    ids = list(range(1, DEEP + 1))
    rng.shuffle(ids)
    lines += [f"MODIFY {i} 5 10000" for i in ids]
    lines.append("PRINT")
    return lines


def main():
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    for name, make in (("mixed", mixed), ("deep_cancel", deep_cancel), ("deep_modify", deep_modify)):
        path = os.path.join(ROOT, "build", f"bench_{name}.txt")
        lines = make()
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        print(f"wrote {len(lines):,} commands to {os.path.relpath(path, ROOT)}")


if __name__ == "__main__":
    main()
