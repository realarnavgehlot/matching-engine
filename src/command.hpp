// Turning one line of the command file into a Command, and running it on the book.
// Shared by the engine (src/main.cpp) and the benchmark (bench/bench.cpp).

#pragma once

#include <cstdint>
#include <string_view>

#include "order_book.hpp"

enum class CommandType { Limit, Market, Modify, Cancel, Print, Reject };

struct Command {
    CommandType type = CommandType::Reject;
    Side side = Side::Buy;         // Limit and Market only
    std::int64_t id = 0;           // Limit, Market, Modify, Cancel
    std::int64_t qty = 0;          // Limit, Market, Modify
    std::int64_t price = 0;        // Limit, Modify
    const char* reason = nullptr;  // Reject only: "unknown_command" or "bad_format"
};

// False for lines that are not commands: empty, only spaces or tabs, or a comment.
// Those lines get no sequence number.
// (A std::string_view is a view of characters owned by someone else, here the line: it points
// at them without copying. A std::string can be passed wherever a std::string_view is expected.)
bool isCommandLine(std::string_view line);

// Parse one command line. A line with the wrong shape becomes a Reject command.
// Checks on the values themselves (bad_id, duplicate_id, ...) are done by the book.
Command parseCommand(std::string_view line);

// Run one command on the book, with the given sequence number.
void runCommand(OrderBook& book, std::uint64_t seq, const Command& cmd);
