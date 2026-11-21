// Matching engine v1: reads a command file and runs each command on the order book.
// The input format and every output line are defined in docs/SPEC.md.

#include <charconv>      // std::from_chars
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>  // std::errc
#include <vector>

#include "order_book.hpp"

namespace {

// True for lines that are not commands: empty, only spaces or tabs, or a comment.
bool isBlankOrComment(const std::string& line) {
    std::size_t first = line.find_first_not_of(" \t\r");
    return first == std::string::npos || line[first] == '#';
}

// Reads the rest of the line into `numbers`. Returns true only if there are exactly `count` words
// and each one is a whole number (digits, optionally a leading '-') that fits in std::int64_t.
bool readNumbers(std::istringstream& in, std::size_t count, std::vector<std::int64_t>& numbers) {
    numbers.clear();
    std::string word;
    while (in >> word) {
        std::int64_t value = 0;
        const char* first = word.data();
        const char* last = first + word.size();
        auto [end, error] = std::from_chars(first, last, value);
        if (error != std::errc() || end != last) {  // not a number, too big, or "5x"
            return false;
        }
        numbers.push_back(value);
    }
    return numbers.size() == count;
}

}  // namespace

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
    std::vector<std::int64_t> n;  // the numbers after the command word

    while (std::getline(file, line)) {
        if (isBlankOrComment(line)) {
            continue;  // no sequence number
        }
        ++seq;  // every other line is a command, even an invalid one

        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "BUY" || cmd == "SELL") {
            if (!readNumbers(in, 3, n)) {
                book.reject(seq, "bad_format");
            } else {
                Side side = (cmd == "BUY") ? Side::Buy : Side::Sell;
                book.limit(seq, side, n[0], n[1], n[2]);
            }
        } else if (cmd == "MARKET_BUY" || cmd == "MARKET_SELL") {
            if (!readNumbers(in, 2, n)) {
                book.reject(seq, "bad_format");
            } else {
                Side side = (cmd == "MARKET_BUY") ? Side::Buy : Side::Sell;
                book.market(seq, side, n[0], n[1]);
            }
        } else if (cmd == "MODIFY") {
            if (!readNumbers(in, 3, n)) {
                book.reject(seq, "bad_format");
            } else {
                book.modify(seq, n[0], n[1], n[2]);
            }
        } else if (cmd == "CANCEL") {
            if (!readNumbers(in, 1, n)) {
                book.reject(seq, "bad_format");
            } else {
                book.cancel(seq, n[0]);
            }
        } else if (cmd == "PRINT") {
            if (!readNumbers(in, 0, n)) {
                book.reject(seq, "bad_format");
            } else {
                book.print(seq);
            }
        } else {
            book.reject(seq, "unknown_command");
        }
    }

    return 0;
}
