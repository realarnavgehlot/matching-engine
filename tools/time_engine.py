"""Time the whole engine on a command file, from the outside: start engine.exe, wait for it to
finish, and repeat. Reading, parsing, the book and writing the output are all included.
The clock is here, not in the engine (CLAUDE.md rule 6).

Output goes to build/time_engine_out.txt, so writing to disk is included but the terminal isn't.

Usage, from the matching-engine folder:
    python tools/time_engine.py build/bench_mixed.txt
    python tools/time_engine.py build/bench_mixed.txt 9      (9 runs instead of 5)
"""

import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "engine.exe" if os.name == "nt" else "engine")


def main():
    if len(sys.argv) < 2:
        print("usage: python tools/time_engine.py <command file> [runs]", file=sys.stderr)
        sys.exit(1)
    path = sys.argv[1]
    runs = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    out_path = os.path.join(ROOT, "build", "time_engine_out.txt")

    times = []
    for _ in range(runs):
        with open(out_path, "wb") as out:
            start = time.perf_counter()
            subprocess.run([EXE, path], stdout=out, check=True)
            times.append(time.perf_counter() - start)
    times.sort()
    size = os.path.getsize(out_path)
    print(
        f"{path}: best {times[0]:.3f} s, median {times[len(times) // 2]:.3f} s "
        f"over {runs} runs ({size:,} bytes of output)"
    )


if __name__ == "__main__":
    main()
