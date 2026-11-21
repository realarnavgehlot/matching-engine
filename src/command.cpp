#include "command.hpp"

#include <charconv>      // std::from_chars
#include <system_error>  // std::errc

namespace {

// The characters that separate words: the same set `std::istringstream >>` uses (the "C"
// locale's isspace), so this parser accepts and rejects exactly the lines the v1 one did.
bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A command word and at most 3 numbers. One more slot lets us see "too many words".
constexpr std::size_t kMaxWords = 5;

// Split `line` into words without copying anything: each word is a std::string_view that points
// into `line`. Keeps the first kMaxWords words and returns how many words there really are.
std::size_t splitWords(std::string_view line, std::string_view (&words)[kMaxWords]) {
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && isSpace(line[i])) {
            ++i;
        }
        if (i == line.size()) {
            break;
        }
        std::size_t start = i;
        while (i < line.size() && !isSpace(line[i])) {
            ++i;
        }
        if (count < kMaxWords) {
            words[count] = line.substr(start, i - start);
        }
        ++count;
    }
    return count;
}

// Reads one word as a whole number (digits, optionally a leading '-') that fits in std::int64_t.
bool toNumber(std::string_view word, std::int64_t& value) {
    const char* first = word.data();
    const char* last = first + word.size();
    auto [end, error] = std::from_chars(first, last, value);
    return error == std::errc() && end == last;  // not a number, too big, or "5x" all fail
}

// Reads words[1] .. words[count] into `numbers`. Returns false unless the line has exactly
// `count` words after the command word and each one is a number.
bool readNumbers(const std::string_view (&words)[kMaxWords], std::size_t wordCount,
                 std::size_t count, std::int64_t (&numbers)[3]) {
    if (wordCount != count + 1) {
        return false;
    }
    for (std::size_t k = 0; k < count; ++k) {
        if (!toNumber(words[k + 1], numbers[k])) {
            return false;
        }
    }
    return true;
}

Command reject(const char* reason) {
    Command cmd;
    cmd.type = CommandType::Reject;
    cmd.reason = reason;
    return cmd;
}

}  // namespace

bool isCommandLine(std::string_view line) {
    std::size_t first = line.find_first_not_of(" \t\r");
    return first != std::string_view::npos && line[first] != '#';
}

Command parseCommand(std::string_view line) {
    std::string_view words[kMaxWords];
    std::size_t wordCount = splitWords(line, words);
    std::string_view word = (wordCount > 0) ? words[0] : std::string_view();

    std::int64_t n[3] = {0, 0, 0};  // the numbers after the command word
    Command cmd;

    if (word == "BUY" || word == "SELL") {
        if (!readNumbers(words, wordCount, 3, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Limit;
        cmd.side = (word == "BUY") ? Side::Buy : Side::Sell;
        cmd.id = n[0];
        cmd.qty = n[1];
        cmd.price = n[2];
    } else if (word == "MARKET_BUY" || word == "MARKET_SELL") {
        if (!readNumbers(words, wordCount, 2, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Market;
        cmd.side = (word == "MARKET_BUY") ? Side::Buy : Side::Sell;
        cmd.id = n[0];
        cmd.qty = n[1];
    } else if (word == "MODIFY") {
        if (!readNumbers(words, wordCount, 3, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Modify;
        cmd.id = n[0];
        cmd.qty = n[1];
        cmd.price = n[2];
    } else if (word == "CANCEL") {
        if (!readNumbers(words, wordCount, 1, n)) {
            return reject("bad_format");
        }
        cmd.type = CommandType::Cancel;
        cmd.id = n[0];
    } else if (word == "PRINT") {
        if (!readNumbers(words, wordCount, 0, n)) {
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
