// Benchmark: how fast the order book runs a command file.
//
// The clock lives here, in the benchmark, never in the engine (CLAUDE.md rule 6).
// Output is formatted as usual but thrown away, so writing to the terminal isn't what we measure.
//
// Build (from the matching-engine folder):
//   g++ -std=c++17 -Wall -Wextra -O2 -o bench.exe bench/bench.cpp src/command.cpp src/order_book.cpp
// Run:
//   .\bench.exe build\bench_mixed.txt
//   .\bench.exe build\bench_mixed.txt 10      (10 timed runs instead of 5)

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <streambuf>
#include <string>
#include <vector>

#include "../src/command.hpp"
#include "../src/order_book.hpp"

namespace {

using Clock = std::chrono::steady_clock;

std::int64_t nanosBetween(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

// A stream buffer that throws output away but counts the bytes. The book still formats every
// line (that's part of the work), and the count proves the output really happened.
class CountingBuffer : public std::streambuf {
public:
    std::uint64_t bytes = 0;

protected:
    int_type overflow(int_type c) override {
        if (!traits_type::eq_int_type(c, traits_type::eof())) {
            ++bytes;
        }
        return traits_type::not_eof(c);
    }
    std::streamsize xsputn(const char*, std::streamsize n) override {
        bytes += static_cast<std::uint64_t>(n);
        return n;
    }
};

// Value at percentile p (0 to 1) of a sorted list, by the nearest-rank method.
std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    std::size_t rank = static_cast<std::size_t>(p * static_cast<double>(sorted.size()));
    if (rank >= sorted.size()) {
        rank = sorted.size() - 1;
    }
    return sorted[rank];
}

const char* typeName(CommandType type) {
    switch (type) {
        case CommandType::Limit: return "limit";
        case CommandType::Market: return "market";
        case CommandType::Modify: return "modify";
        case CommandType::Cancel: return "cancel";
        case CommandType::Print: return "print";
        case CommandType::Reject: return "reject";
    }
    return "?";
}

// Run every command once on a fresh book. Returns the total time in nanoseconds.
std::int64_t runAll(const std::vector<Command>& commands, std::uint64_t& outputBytes) {
    CountingBuffer buffer;
    std::ostream out(&buffer);
    OrderBook book(out);
    Clock::time_point start = Clock::now();
    for (std::size_t i = 0; i < commands.size(); ++i) {
        runCommand(book, i + 1, commands[i]);
    }
    Clock::time_point end = Clock::now();
    outputBytes = buffer.bytes;
    return nanosBetween(start, end);
}

void printRow(const std::string& name, std::vector<std::int64_t>& times) {
    std::sort(times.begin(), times.end());
    std::cout << "  " << std::left << std::setw(8) << name << std::right << std::setw(10)
              << times.size() << std::setw(10) << percentile(times, 0.50) << std::setw(10)
              << percentile(times, 0.90) << std::setw(10) << percentile(times, 0.99)
              << std::setw(10) << percentile(times, 0.999) << std::setw(12)
              << (times.empty() ? 0 : times.back()) << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: bench.exe <command file> [timed runs, default 5]\n";
        return 1;
    }
    int runs = (argc >= 3) ? std::atoi(argv[2]) : 5;
    if (runs < 1) {
        runs = 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "error: could not open " << argv[1] << "\n";
        return 1;
    }

    // 1. Read and parse every line before timing the book, so the book's time is the book's alone.
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (isCommandLine(line)) {
            lines.push_back(line);
        }
    }
    Clock::time_point parseStart = Clock::now();
    std::vector<Command> commands;
    commands.reserve(lines.size());
    for (const std::string& text : lines) {
        commands.push_back(parseCommand(text));
    }
    std::int64_t parseNanos = nanosBetween(parseStart, Clock::now());
    double n = static_cast<double>(commands.size());

    std::cout << "file:      " << argv[1] << "\n";
    std::cout << "commands:  " << commands.size() << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "parse:     " << parseNanos / 1e6 << " ms (" << parseNanos / n
              << " ns per command)\n\n";

    // 2. Throughput: the whole file, start to finish, several times on a fresh book each time.
    std::vector<std::int64_t> totals;
    std::uint64_t outputBytes = 0;
    for (int r = 0; r < runs; ++r) {
        totals.push_back(runAll(commands, outputBytes));
    }
    std::sort(totals.begin(), totals.end());
    std::int64_t best = totals.front();
    std::int64_t median = totals[totals.size() / 2];
    std::cout << "book throughput over " << runs << " runs (output formatted, then discarded: "
              << outputBytes << " bytes per run)\n";
    std::cout << "  best:    " << best / 1e6 << " ms = " << n / (best / 1e9) / 1e6
              << " million commands per second\n";
    std::cout << "  median:  " << median / 1e6 << " ms = " << n / (median / 1e9) / 1e6
              << " million commands per second\n\n";

    // 3. Latency: one more run, timing every command on its own.
    //    First find the clock's resolution: the smallest step it ever moves by. Anything
    //    faster than one step reads as 0 or one step, so small latencies are only rough.
    std::int64_t resolution = -1;
    for (int i = 0; i < 1000; ++i) {
        Clock::time_point a = Clock::now();
        Clock::time_point b = Clock::now();
        while (b == a) {
            b = Clock::now();
        }
        std::int64_t step = nanosBetween(a, b);
        if (resolution < 0 || step < resolution) {
            resolution = step;
        }
    }

    std::vector<std::int64_t> all;
    std::vector<std::vector<std::int64_t>> byType(6);
    all.reserve(commands.size());
    {
        CountingBuffer buffer;
        std::ostream out(&buffer);
        OrderBook book(out);
        for (std::size_t i = 0; i < commands.size(); ++i) {
            Clock::time_point a = Clock::now();
            runCommand(book, i + 1, commands[i]);
            Clock::time_point b = Clock::now();
            std::int64_t t = nanosBetween(a, b);
            all.push_back(t);
            byType[static_cast<int>(commands[i].type)].push_back(t);
        }
    }

    std::cout << "latency per command in ns (the clock moves in steps of " << resolution
              << " ns, so values near that are rough; throughput above is the better measure)\n";
    std::cout << "  " << std::left << std::setw(8) << "type" << std::right << std::setw(10)
              << "count" << std::setw(10) << "p50" << std::setw(10) << "p90" << std::setw(10)
              << "p99" << std::setw(10) << "p99.9" << std::setw(12) << "max" << "\n";
    printRow("all", all);
    for (int t = 0; t < 6; ++t) {
        if (!byType[t].empty()) {
            printRow(typeName(static_cast<CommandType>(t)), byType[t]);
        }
    }
    return 0;
}
