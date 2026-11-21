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
//
// Expiry (v3, Step 18): the engine's clock is logical; only TIME commands move it. Resting GTT
// orders are also kept in `expiries_`, a std::map sorted by (GTT time, sequence number), so the
// order to expire next is always at its front.

#pragma once

#include <cstddef>        // std::size_t
#include <cstdint>        // std::int64_t, std::uint64_t, std::uint32_t
#include <functional>     // std::greater
#include <map>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>        // std::pair
#include <vector>

#include "output.hpp"

enum class Side { Buy, Sell };

// What happens to the part of a limit order that can't trade at once: its "time in force".
enum class TimeInForce {
    GoodTillCancel,     // no keyword: rests until filled or cancelled
    GoodTillTime,       // GTT <time>: rests, but leaves when the clock reaches its time
    ImmediateOrCancel,  // IOC: never rests; whatever can't trade at once is dropped
    FillOrKill,         // FOK: trades its whole quantity at once, or nothing at all
};

// The hash function for the book's hash tables (v3, Step 19). Normally std::hash. Building with
// -DENGINE_SCRAMBLED_HASH swaps in a very different one, so every entry lands in a different
// bucket and the tables' internal order changes completely. run_tests.py builds the engine both
// ways and requires identical output: if it ever differs, some code depends on hash-table order,
// which rule 5 in CLAUDE.md forbids.
#ifdef ENGINE_SCRAMBLED_HASH
struct IdHash {
    std::size_t operator()(std::int64_t id) const noexcept {
        // The "fmix64" step of MurmurHash3: mixes every bit of the id into every bit of the result.
        std::uint64_t x = static_cast<std::uint64_t>(id);
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33;
        x *= 0xc4ceb9fe1a85ec53ULL;
        x ^= x >> 33;
        return static_cast<std::size_t>(x);
    }
};
#else
using IdHash = std::hash<std::int64_t>;
#endif

// A position in the pool. kNoSlot means "no order" (like a null pointer).
using Slot = std::uint32_t;
constexpr Slot kNoSlot = 0xFFFFFFFFu;

// One resting order.
struct Order {
    std::int64_t id;
    std::int64_t qty;    // quantity still left
    std::int64_t price;
    std::uint64_t seq;   // sequence number of the command that put it in the book
    std::optional<std::int64_t> expiry;  // its GTT time, or empty if it never expires
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
    // Every output line is written to `out`. Output is buffered: call flush() when done
    // (the destructor also flushes).
    //
    // `expectedOrders` is a hint: roughly how many orders will arrive. The hash tables are sized
    // for that many up front, so they don't have to grow, and move every entry each time they
    // do, along the way. A wrong guess only costs speed; it never changes the output, because
    // the hash tables are never looped over (CLAUDE.md rule 5).
    explicit OrderBook(std::ostream& out, std::size_t expectedOrders = 0);

    // Write any buffered output to the stream now.
    void flush();

    // BUY or SELL: match, then what happens to the rest depends on `tif` (rules 8, 14, 18, 19).
    // `expiry` is the GTT time, used only when tif is GoodTillTime.
    void limit(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty, std::int64_t price,
               TimeInForce tif = TimeInForce::GoodTillCancel, std::int64_t expiry = 0);

    // TIME: move the logical clock to `time` and expire every order whose time has come.
    void setTime(std::uint64_t seq, std::int64_t time);

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
    void enter(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty, std::int64_t price,
               std::optional<std::int64_t> expiry);

    // Remove a resting order from `expiries_`, if it has a GTT time.
    void forgetExpiry(const Order& order);

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
              std::int64_t price, std::optional<std::int64_t> expiry);
    template <typename Book>
    void unlink(Book& book, Slot slot);
    template <typename Book>
    bool canFill(const Book& opposite, Side side, std::int64_t qty, std::int64_t limit) const;
    template <typename Book>
    void printSide(const Book& book, const char* name) const;

    BidBook bids_;
    AskBook asks_;
    std::vector<Order> pool_;  // every order, resting or freed; named by slot number
    std::vector<Slot> free_;   // slots of freed orders, ready to reuse
    std::int64_t now_ = 0;     // the logical clock (rule 13)
    // Every resting GTT order, keyed by (GTT time, sequence number): the front expires first.
    std::map<std::pair<std::int64_t, std::uint64_t>, Slot> expiries_;
    // Hash containers are only used to look things up by key, never looped over (CLAUDE.md rule 5).
    std::unordered_map<std::int64_t, Slot, IdHash> index_;  // id of each resting order -> its slot
    std::unordered_set<std::int64_t, IdHash> usedIds_;      // every id an accepted order has used
    // `mutable` so that print(), which doesn't change the book, can still write output.
    mutable Output out_;
};
