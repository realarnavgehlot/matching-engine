"""Build the engine, run every test in tests/, and compare with the expected output.

Usage, from the matching-engine folder:
    python run_tests.py

A test is a pair of files in tests/:
    name.in.txt        the commands to feed the engine
    name.expected.txt  exactly what the engine should print

Every test runs twice. If the two outputs differ, the engine is not deterministic.
"""

import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "engine.exe" if os.name == "nt" else "engine")
TIMEOUT_SECONDS = 5


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


def normalize(text):
    """Ignore line-ending differences, spaces at the ends of lines, and blank lines at the end."""
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").split("\n")]
    while lines and lines[-1] == "":
        lines.pop()
    return lines


def run_engine(input_path):
    """Run the engine on one command file. Returns (output lines, error message or None)."""
    try:
        result = subprocess.run(
            [EXE, input_path], capture_output=True, text=True, timeout=TIMEOUT_SECONDS
        )
    except subprocess.TimeoutExpired:
        return None, f"still running after {TIMEOUT_SECONDS} seconds (an infinite loop?)"
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
        if error:
            print(f"FAIL  {name}: {error}")
        elif first == expected:
            print(f"PASS  {name}")
            passed += 1
        else:
            print(f"FAIL  {name}")
            show_difference(expected, first)

    print(f"\n{passed}/{len(tests)} tests passed")
    sys.exit(0 if passed == len(tests) else 1)


if __name__ == "__main__":
    main()
