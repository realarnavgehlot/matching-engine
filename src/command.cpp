#include "command.hpp"

#include <charconv>      // std::from_chars
#include <sstream>
#include <system_error>  // std::errc
#include <vector>

namespace {

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

Command reject(const char* reason) {
    Command cmd;
    cmd.type = CommandType::Reject;
    cmd.reason = reason;
    return cmd;
}

}  // namespace

bool isCommandLine(const std::string& line) {
    std::size_t first = line.find_first_not_of(" \t\r");
    return first != std::string::npos && line[first] != '#';
}

Command parseCommand(const std::string& line) {
    std::istringstream in(line);
    std::string word;
    in >> word;

    std::vector<std::int64_t> n;  // the numbers after the command word
    Command cmd;

    if (word == "BUY" || word == "SELL") {
        if (!readNumbers(in, 3, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Limit;
        cmd.side = (word == "BUY") ? Side::Buy : Side::Sell;
        cmd.id = n[0];
        cmd.qty = n[1];
        cmd.price = n[2];
    } else if (word == "MARKET_BUY" || word == "MARKET_SELL") {
        if (!readNumbers(in, 2, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Market;
        cmd.side = (word == "MARKET_BUY") ? Side::Buy : Side::Sell;
        cmd.id = n[0];
        cmd.qty = n[1];
    } else if (word == "MODIFY") {
        if (!readNumbers(in, 3, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Modify;
        cmd.id = n[0];
        cmd.qty = n[1];
        cmd.price = n[2];
    } else if (word == "CANCEL") {
        if (!readNumbers(in, 1, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Cancel;
        cmd.id = n[0];
    } else if (word == "PRINT") {
        if (!readNumbers(in, 0, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Print;
    } else {
        return reject("unknown_command");
    }
    return cmd;
}

void runCommand(OrderBook& book, std::uint64_t seq, const Command& cmd) {
    switch (cmd.type) {
        case CommandType::Limit:
            book.limit(seq, cmd.side, cmd.id, cmd.qty, cmd.price);
            break;
        case CommandType::Market:
            book.market(seq, cmd.side, cmd.id, cmd.qty);
            break;
        case CommandType::Modify:
            book.modify(seq, cmd.id, cmd.qty, cmd.price);
            break;
        case CommandType::Cancel:
            book.cancel(seq, cmd.id);
            break;
        case CommandType::Print:
            book.print(seq);
            break;
        case CommandType::Reject:
            book.reject(seq, cmd.reason);
            break;
    }
}
