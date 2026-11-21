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

    // Guess how many orders the file holds from its size (a command line is about 16-20 bytes),
    // so the book can size its hash tables once. The guess affects speed only, never output.
    file.seekg(0, std::ios::end);
    std::streamoff bytes = file.tellg();  // -1 if the size can't be found
    file.seekg(0, std::ios::beg);
    std::size_t expectedOrders = (bytes > 0) ? static_cast<std::size_t>(bytes / 16) : 0;

    OrderBook book(std::cout, expectedOrders);
    std::uint64_t seq = 0;  // sequence number of the current command
    std::string line;

    while (std::getline(file, line)) {
        if (!isCommandLine(line)) {
            continue;  // blank or comment: no sequence number
        }
        ++seq;  // every other line is a command, even an invalid one
        runCommand(book, seq, parseCommand(line));
    }

    book.flush();  // write out whatever is still in the output buffer
    return 0;
}
