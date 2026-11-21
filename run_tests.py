"""Build the engine, run every test in tests/, and compare with the expected output.

Usage, from the matching-engine folder:
    python run_tests.py

A test is a pair of files in tests/:
    name.in.txt        the commands to feed the engine
    name.expected.txt  exactly what the engine should print

Every test runs twice. If the two outputs differ, the engine is not deterministic.

Every test is also run through tools/reference_engine.py, a second, independent
implementation of docs/SPEC.md, which must match the expected output too.

A second engine is built with -DENGINE_SCRAMBLED_HASH: its hash tables use a very different
hash function and no size hint, so their internal order is completely different. Every test
and stress file must give exactly the same output from both builds; if not, some code depends
on hash-table order (rule 5 in CLAUDE.md).

After the tests, the stress checks generate random commands with fixed seeds
(tools/gen_commands.py). For each seed the engine runs twice, and the check fails if:
- the two runs differ (not deterministic),
- the scrambled-hash build prints anything different,
- any line is malformed, or any printed book is unsorted or crossed, or
- the engine's output differs from the reference engine's in any line (differential test).
Skip them with:
    python run_tests.py --no-stress
"""

import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "engine.exe" if os.name == "nt" else "engine")
SCRAMBLED_EXE = os.path.join(
    ROOT, "engine_scrambled.exe" if os.name == "nt" else "engine_scrambled"
)
HASH_ORDER_ERROR = (
    "the scrambled-hash build printed different output, so the output depends on "
    "hash-table order (rule 5 in CLAUDE.md)"
)
TIMEOUT_SECONDS = 5
STRESS_TIMEOUT_SECONDS = 60
# (seed, number of commands, PRINT every N commands). The small runs print the book often,
# so the reference comparison also checks the whole book many times.
STRESS_RUNS = [
    (1, 100_000, 10_000),
    (2, 20_000, 250),
    (3, 20_000, 250),
    (4, 20_000, 250),
]

sys.path.insert(0, os.path.join(ROOT, "tools"))
from gen_commands import generate  # noqa: E402
import reference_engine  # noqa: E402


def build():
    sources = sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp")))
    if not sources:
        print("No .cpp files in src/ yet. Start with Step 0 in docs/STEPS.md.")
        sys.exit(1)
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-O2", "-o", EXE] + sources
    print("Building the engine...")
    try:
        result = subprocess.run(command, cwd=ROOT)
    except FileNotFoundError:
        print("g++ was not found. See Step 0 in docs/STEPS.md.")
        sys.exit(1)
    if result.returncode != 0:
        print("Build failed. Fix the compiler errors above, then run the tests again.")
        sys.exit(1)

    # The benchmark uses the same book, so build it too: a change in src/ must not break it.
    bench_sources = [os.path.join(ROOT, "bench", "bench.cpp")] + [
        s for s in sources if os.path.basename(s) != "main.cpp"
    ]
    # (Not plain "bench" on Linux: that's the name of the folder bench/ itself.)
    bench_exe = os.path.join(ROOT, "bench.exe" if os.name == "nt" else "bench_engine")
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-O2", "-o", bench_exe] + bench_sources
    if subprocess.run(command, cwd=ROOT).returncode != 0:
        print("The benchmark (bench/bench.cpp) failed to build. Fix the errors above.")
        sys.exit(1)

    # The same engine with a scrambled hash function (see the top of this file).
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-O2", "-DENGINE_SCRAMBLED_HASH",
               "-o", SCRAMBLED_EXE] + sources
    if subprocess.run(command, cwd=ROOT).returncode != 0:
        print("The scrambled-hash build failed. Fix the errors above.")
        sys.exit(1)


def normalize(text):
    """Ignore line-ending differences, spaces at the ends of lines, and blank lines at the end."""
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").split("\n")]
    while lines and lines[-1] == "":
        lines.pop()
    return lines


def run_engine(input_path, timeout=TIMEOUT_SECONDS, exe=EXE):
    """Run the engine on one command file. Returns (output lines, error message or None)."""
    try:
        result = subprocess.run(
            [exe, input_path], capture_output=True, text=True, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        return None, f"still running after {timeout} seconds (an infinite loop?)"
    if result.returncode != 0:
        message = f"the engine exited with code {result.returncode}"
        details = result.stderr.strip()
        return None, message + (f": {details}" if details else "")
    return normalize(result.stdout), None


def show_difference(expected, actual):
    for i in range(max(len(expected), len(actual))):
        want = expected[i] if i < len(expected) else "(no line)"
        got = actual[i] if i < len(actual) else "(no line)"
        if want != got:
            print(f"      first difference, at line {i + 1}:")
            print(f"        expected:    {want}")
            print(f"        your output: {got}")
            break
    print("      full expected output:")
    for line in expected:
        print("        " + line)
    print("      your full output:")
    for line in actual or ["(nothing)"]:
        print("        " + line)


# Every line the engine may print, from the Output table in docs/SPEC.md.
LINE_PATTERNS = [
    re.compile(r"\[(\d+)\] TRADE buy=(\d+) sell=(\d+) qty=(\d+) price=(\d+)"),
    re.compile(r"\[(\d+)\] UNFILLED id=\d+ qty=[1-9]\d*"),
    re.compile(r"\[(\d+)\] EXPIRED id=\d+ qty=[1-9]\d*"),
    re.compile(r"\[(\d+)\] MODIFIED id=\d+ qty=[1-9]\d* price=[1-9]\d*"),
    re.compile(r"\[(\d+)\] (MODIFY_REJECTED|CANCELLED|CANCEL_REJECTED) id=-?\d+"),
    re.compile(
        r"\[(\d+)\] REJECTED reason="
        r"(unknown_command|bad_format|bad_id|bad_quantity|bad_price|bad_expiry|duplicate_id"
        r"|time_backwards)"
    ),
    re.compile(r"\[(\d+)\] BOOK"),
]
LEVEL_PATTERN = re.compile(r"  (ASK|BID) ([1-9]\d*):((?: \d+x[1-9]\d*)+)")


def check_book(levels):
    """Check one printed book. Returns an error message or None."""
    asks = [price for side, price in levels if side == "ASK"]
    bids = [price for side, price in levels if side == "BID"]
    sides = [side for side, _ in levels]
    if sides != sorted(sides):  # "ASK" sorts before "BID"
        return "bid levels printed before ask levels"
    if asks != sorted(set(asks)):
        return "ask levels are not strictly lowest price first"
    if bids != sorted(set(bids), reverse=True):
        return "bid levels are not strictly highest price first"
    if asks and bids and bids[0] >= asks[0]:
        return f"book is crossed: best bid {bids[0]} >= best ask {asks[0]}"
    return None


def check_stress_output(lines, command_count):
    """Check every line of the stress output. Returns an error message or None."""
    last_seq = 0
    i = 0
    while i < len(lines):
        line = lines[i]
        match = next((m for p in LINE_PATTERNS for m in [p.fullmatch(line)] if m), None)
        if match is None:
            return f"line {i + 1} is not a valid output line: {line!r}"
        seq = int(match.group(1))
        if seq < last_seq or seq > command_count:
            return f"line {i + 1} has sequence number {seq} out of order"
        last_seq = seq
        if " TRADE " in line and match.group(2) == match.group(3):
            return f"line {i + 1}: an order traded with itself"
        i += 1
        if line.endswith(" BOOK"):
            levels = []
            ids = set()
            if i < len(lines) and lines[i] == "  (empty)":
                i += 1
                continue
            while i < len(lines) and lines[i].startswith("  "):
                level = LEVEL_PATTERN.fullmatch(lines[i])
                if level is None:
                    return f"line {i + 1} is not a valid book line: {lines[i]!r}"
                for entry in level.group(3).split():
                    order_id = entry.split("x")[0]
                    if order_id in ids:
                        return f"line {i + 1}: order {order_id} appears twice in the book"
                    ids.add(order_id)
                levels.append((level.group(1), int(level.group(2))))
                i += 1
            if not levels:
                return f"line {i}: BOOK with no levels and no (empty)"
            error = check_book(levels)
            if error:
                return f"book printed at [{seq}]: {error}"
    return None


def compare_with_reference(engine_lines, reference_lines, command_path):
    """Returns an error message describing the first difference, or None if they match."""
    for i in range(max(len(engine_lines), len(reference_lines))):
        mine = engine_lines[i] if i < len(engine_lines) else "(no line)"
        ref = reference_lines[i] if i < len(reference_lines) else "(no line)"
        if mine != ref:
            return (
                f"engine and reference differ at output line {i + 1}\n"
                f"        engine:    {mine}\n"
                f"        reference: {ref}\n"
                f"      replay it: .\\engine.exe {os.path.relpath(command_path, ROOT)}"
            )
    return None


def stress(seed, count, print_every):
    """Run one seeded stress check. Returns True if it passed."""
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)
    path = os.path.join(ROOT, "build", f"stress_seed{seed}.txt")
    commands = generate(seed, count, print_every)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(commands) + "\n")

    name = f"stress seed {seed} ({len(commands):,} commands)"
    first, error = run_engine(path, STRESS_TIMEOUT_SECONDS)
    if error is None:
        second, error = run_engine(path, STRESS_TIMEOUT_SECONDS)
        if error is None and second != first:
            error = "two runs gave different output, so the engine is not deterministic"
    if error is None:
        scrambled, error = run_engine(path, STRESS_TIMEOUT_SECONDS, SCRAMBLED_EXE)
        if error is None and scrambled != first:
            error = HASH_ORDER_ERROR
    if error is None:
        error = check_stress_output(first, len(commands))
    if error is None:
        error = compare_with_reference(first, reference_engine.run(commands), path)
    if error:
        print(f"FAIL  {name}: {error}")
        return False
    trades = sum(1 for line in first if " TRADE " in line)
    books = sum(1 for line in first if line.endswith(" BOOK"))
    print(
        f"PASS  {name}: {len(first):,} lines, {trades:,} trades, {books} books, "
        "same as reference and scrambled hash"
    )
    return True


def main():
    build()
    tests = sorted(glob.glob(os.path.join(ROOT, "tests", "*.in.txt")))
    if not tests:
        print("No tests found in tests/.")
        sys.exit(1)

    passed = 0
    for input_path in tests:
        name = os.path.basename(input_path)[: -len(".in.txt")]
        expected_path = input_path[: -len(".in.txt")] + ".expected.txt"
        if not os.path.exists(expected_path):
            print(f"FAIL  {name}: tests/{name}.expected.txt is missing")
            continue
        with open(expected_path, encoding="utf-8") as f:
            expected = normalize(f.read())

        first, error = run_engine(input_path)
        if error is None:
            second, error = run_engine(input_path)
            if error is None and second != first:
                error = "two runs gave different output, so the engine is not deterministic"
        if error is None:
            scrambled, error = run_engine(input_path, exe=SCRAMBLED_EXE)
            if error is None and scrambled != first:
                error = HASH_ORDER_ERROR
        reference = reference_engine.run_file(input_path)
        if error:
            print(f"FAIL  {name}: {error}")
        elif first != expected:
            print(f"FAIL  {name}")
            show_difference(expected, first)
        elif reference != expected:
            print(f"FAIL  {name}: the engine is right, but tools/reference_engine.py is not")
            show_difference(expected, reference)
        else:
            print(f"PASS  {name}")
            passed += 1

    print(f"\n{passed}/{len(tests)} tests passed")
    all_passed = passed == len(tests)

    if "--no-stress" not in sys.argv:
        print()
        for seed, count, print_every in STRESS_RUNS:
            all_passed = stress(seed, count, print_every) and all_passed

    sys.exit(0 if all_passed else 1)


if __name__ == "__main__":
    main()
