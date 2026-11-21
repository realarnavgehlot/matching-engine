// The order book: resting orders, matching, and all output lines.
// The rules it follows are in docs/SPEC.md.

#pragma once

#include <cstdint>        // std::int64_t, std::uint64_t
#include <deque>
#include <functional>     // std::greater
#include <map>
#include <ostream>
#include <unordered_map>
#include <unordered_set>

enum class Side { Buy, Sell };

// One order resting in the book.
struct Order {
    std::int64_t id;
    std::int64_t qty;   // quantity still left
    std::uint64_t seq;  // sequence number of the command that placed it (for debugging)
};

// All orders at one price, in arrival order. The front order trades first (rule 4).
using Level = std::deque<Order>;

// Where a resting order sits, so CANCEL and MODIFY can find it without searching the whole book.
struct Location {
    Side side;
    std::int64_t price;
};

class OrderBook {
public:
    // Every output line is written to `out`.
    explicit OrderBook(std::ostream& out);

    // BUY or SELL: match, then any rest joins the book.
    void limit(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty, std::int64_t price);

    // MARKET_BUY or MARKET_SELL: match at any price, then drop any rest.
    void market(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty);

    void modify(std::uint64_t seq, std::int64_t id, std::int64_t qty, std::int64_t price);
    void cancel(std::uint64_t seq, std::int64_t id);
    void print(std::uint64_t seq) const;

    // Print `[seq] REJECTED reason=<reason>` for an invalid command.
    void reject(std::uint64_t seq, const char* reason);

private:
    using BidBook = std::map<std::int64_t, Level, std::greater<std::int64_t>>;  // highest first
    using AskBook = std::map<std::int64_t, Level>;                              // lowest first

    // Match an incoming order, then rest whatever is left (used by limit and modify).
    void enter(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty, std::int64_t price);

    // Match against the other side. Returns the quantity still left.
    std::int64_t matchIncoming(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                               std::int64_t limit);

    // These templates work on either BidBook or AskBook. They are only used in order_book.cpp,
    // so they are defined there.
    template <typename Book>
    std::int64_t match(Book& opposite, std::uint64_t seq, Side side, std::int64_t id,
                       std::int64_t qty, std::int64_t limit);
    template <typename Book>
    void rest(Book& book, Side side, std::uint64_t seq, std::int64_t id, std::int64_t qty,
              std::int64_t price);
    template <typename Book>
    void removeOrder(Book& book, std::int64_t price, std::int64_t id);
    template <typename Book>
    void modifyIn(Book& book, std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                  std::int64_t price, std::int64_t oldPrice);
    template <typename Book>
    void printSide(const Book& book, const char* name) const;

    BidBook bids_;
    AskBook asks_;
    // Hash containers are only used to look things up by key, never looped over (CLAUDE.md rule 5).
    std::unordered_map<std::int64_t, Location> index_;  // id of each resting order -> where it is
    std::unordered_set<std::int64_t> usedIds_;          // every id an accepted order has used
    std::ostream& out_;
};
