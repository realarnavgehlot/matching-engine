// Matching engine: reads a command file and runs each command on the order book.
// The input format and every output line are defined in docs/SPEC.md.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

#include "command.hpp"
#include "order_book.hpp"

int main(int argc, char* argv[]) {
    // The command file must be given: .\engine.exe data\sample.txt
    if (argc < 2) {
        std::cerr << "usage: engine.exe <command file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "error: could not open " << argv[1] << "\n";
        return 1;
    }

    OrderBook book(std::cout);
    std::uint64_t seq = 0;  // sequence number of the current command
    std::string line;

    while (std::getline(file, line)) {
        if (!isCommandLine(line)) {
            continue;  // blank or comment: no sequence number
        }
        ++seq;  // every other line is a command, even an invalid one
        runCommand(book, seq, parseCommand(line));
    }

    return 0;
}
