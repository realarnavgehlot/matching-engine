// The order book: resting orders, matching, and all output lines.
// The rules it follows are in docs/SPEC.md.
//
// How orders are stored (v2, Step 15):
// - Every resting order lives in one big array, the pool, and is named by its slot number.
//   A slot number stays valid when the array grows, unlike a pointer or a reference.
// - The orders at one price form a doubly linked list through their `prev` and `next` slots,
//   in arrival order. A price level only remembers its first and last order.
// - The index maps an order id straight to its slot.
// So CANCEL and MODIFY can unlink any order in a few steps, however long its queue is.
// Slots freed by filled or cancelled orders go on a free list and are reused.

#pragma once

#include <cstdint>        // std::int64_t, std::uint64_t, std::uint32_t
#include <functional>     // std::greater
#include <map>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Side { Buy, Sell };

// A position in the pool. kNoSlot means "no order" (like a null pointer).
using Slot = std::uint32_t;
constexpr Slot kNoSlot = 0xFFFFFFFFu;

// One resting order.
struct Order {
    std::int64_t id;
    std::int64_t qty;    // quantity still left
    std::int64_t price;
    std::uint64_t seq;   // sequence number of the command that placed it (for debugging)
    Side side;
    Slot prev;           // the order in front of this one at the same price, or kNoSlot
    Slot next;           // the order behind it, or kNoSlot
};

// One price level: the two ends of its queue.
struct Level {
    Slot head = kNoSlot;  // the order that trades first (rule 4)
    Slot tail = kNoSlot;  // new orders join behind this one (rule 8)
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

    // Take a resting order out of the book completely: its level, the index and the pool.
    void removeResting(Slot slot);

    // The pool: get a free slot for a new order, or give one back.
    Slot allocate(const Order& order);
    void release(Slot slot);

    // These templates work on either BidBook or AskBook. They are only used in order_book.cpp,
    // so they are defined there.
    template <typename Book>
    std::int64_t match(Book& opposite, std::uint64_t seq, Side side, std::int64_t id,
                       std::int64_t qty, std::int64_t limit);
    template <typename Book>
    void rest(Book& book, Side side, std::uint64_t seq, std::int64_t id, std::int64_t qty,
              std::int64_t price);
    template <typename Book>
    void unlink(Book& book, Slot slot);
    template <typename Book>
    void printSide(const Book& book, const char* name) const;

    BidBook bids_;
    AskBook asks_;
    std::vector<Order> pool_;  // every order, resting or freed; named by slot number
    std::vector<Slot> free_;   // slots of freed orders, ready to reuse
    // Hash containers are only used to look things up by key, never looped over (CLAUDE.md rule 5).
    std::unordered_map<std::int64_t, Slot> index_;  // id of each resting order -> its slot
    std::unordered_set<std::int64_t> usedIds_;      // every id an accepted order has used
    std::ostream& out_;
};
