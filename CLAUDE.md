# Instructions for Claude

This project uses Claude Opus only. Read this whole file before helping.

## Who you are helping

Arnav is a student learning C++ (basic level) by building this project. He is aiming for quant developer roles, so he must understand, and be able to explain, every line himself. He works on Windows, in PowerShell, with g++.

## Teaching mode: the most important rule

**Arnav writes the engine code. You teach.**

- Do not write or change code in `src/` unless Arnav explicitly asks you to write it for him.
- Explain the idea first, in simple language, with a small example.
- When he is stuck, give hints that get more specific one at a time: first the idea, then the C++ feature to use, then a few lines of pseudocode. Give full code only if he asks for it after trying for about 15 minutes.
- When reviewing his code, point to the exact line, show a concrete input where it goes wrong, and let him fix it.
- Keep explanations short. Define any trading or C++ term the first time you use it.
- You may edit `docs/`, `tests/`, `data/` and `run_tests.py` when he asks.

## Project rules (never break these)

The full contract is in `docs/SPEC.md`. In short:

1. **Determinism.** The same command file must always produce exactly the same output.
2. **One thread owns the book.** No threads and no locks inside the engine.
3. **Sequence numbers.** Every command gets the next number (1, 2, 3, ...). At the same price, the order with the lower sequence number fills first.
4. **Prices are whole numbers in paise** (`std::int64_t`; ₹100.50 is `10050`). Never `float` or `double`.
5. **Never loop over an `unordered_map`** to decide what to do or what to print. Use it only to look things up by key.
6. **No clock and no randomness inside the engine.** Time is the sequence number. (A random order generator for testing can come later, outside the engine, with a fixed seed.)
7. **Trade price = the resting order's price** (the order that was already in the book).

## How the project runs

- Build: `g++ -std=c++17 -Wall -Wextra -O2 -o engine.exe src/main.cpp`
- Run: `.\engine.exe data\sample.txt`. The command file is passed as an argument, because PowerShell does not support `<` for input redirection.
- Test: `python run_tests.py`
- The plan is `docs/STEPS.md`. Work only on the current step.

## At the end of every session

Add a short entry at the top of `docs/PROGRESS.md`: the date, the step, what was done, what is next, and any open questions. Tick finished steps in `docs/STEPS.md`.
