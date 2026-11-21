#include "order_book.hpp"

#include <algorithm>  // std::min
#include <limits>     // std::numeric_limits

OrderBook::OrderBook(std::ostream& out, std::size_t expectedOrders) : out_(out) {
    // Growing a hash table means moving every entry into a bigger one. With hundreds of
    // thousands of ids, one such step took over 10 ms (docs/BENCHMARKS.md, Step 17).
    if (expectedOrders > 0) {
        index_.reserve(expectedOrders);
        usedIds_.reserve(expectedOrders);
    }
}

void OrderBook::flush() {
    out_.flush();
}

// ---------------------------------------------------------------------------
// The pool
// ---------------------------------------------------------------------------

Slot OrderBook::allocate(const Order& order) {
    if (!free_.empty()) {  // reuse a freed slot if there is one
        Slot slot = free_.back();
        free_.pop_back();
        pool_[slot] = order;
        return slot;
    }
    pool_.push_back(order);  // may move the whole array, which is why we hold slots, not pointers
    return static_cast<Slot>(pool_.size() - 1);
}

void OrderBook::release(Slot slot) {
    free_.push_back(slot);
}

// ---------------------------------------------------------------------------
// Templates shared by the bid side and the ask side
// ---------------------------------------------------------------------------

// Trade the incoming order against the other side of the book, best price first.
// Returns the quantity the incoming order still has left.
template <typename Book>
std::int64_t OrderBook::match(Book& opposite, std::uint64_t seq, Side side, std::int64_t id,
                              std::int64_t qty, std::int64_t limit) {
    while (qty > 0 && !opposite.empty()) {
        auto level = opposite.begin();  // the best price on the other side
        std::int64_t levelPrice = level->first;

        bool crosses = (side == Side::Buy) ? (levelPrice <= limit) : (levelPrice >= limit);
        if (!crosses) {
            break;
        }

        Level& queue = level->second;
        Slot slot = queue.head;                            // earliest order at this price
        Order& resting = pool_[slot];
        std::int64_t traded = std::min(qty, resting.qty);  // rule 5

        std::int64_t buyId = (side == Side::Buy) ? id : resting.id;
        std::int64_t sellId = (side == Side::Buy) ? resting.id : id;
        out_ << "[" << seq << "] TRADE buy=" << buyId << " sell=" << sellId << " qty=" << traded
             << " price=" << levelPrice << "\n";  // rule 6

        qty -= traded;
        resting.qty -= traded;

        // Rule 7: a filled order leaves the book, and so does an empty level.
        if (resting.qty == 0) {
            index_.erase(resting.id);
            forgetExpiry(resting);  // rule 16: a filled order never expires
            queue.head = resting.next;  // the next order in line is now first
            if (queue.head != kNoSlot) {
                pool_[queue.head].prev = kNoSlot;
            } else {
                queue.tail = kNoSlot;
            }
            release(slot);
            if (queue.head == kNoSlot) {
                opposite.erase(level);
            }
        }
    }
    return qty;
}

// Put an order at the back of the queue at its price (rule 8).
template <typename Book>
void OrderBook::rest(Book& book, Side side, std::uint64_t seq, std::int64_t id, std::int64_t qty,
                     std::int64_t price, std::optional<std::int64_t> expiry) {
    // Allocate first: it may move the pool, so take no references into it before this.
    Slot slot = allocate(Order{id, qty, price, seq, expiry, side, kNoSlot, kNoSlot});
    if (expiry) {
        expiries_[{*expiry, seq}] = slot;  // (GTT time, sequence number) is unique: see rule 15
    }
    Level& queue = book[price];  // [] creates the level if it's new
    if (queue.tail == kNoSlot) {
        queue.head = slot;
    } else {
        pool_[queue.tail].next = slot;
        pool_[slot].prev = queue.tail;
    }
    queue.tail = slot;
    index_[id] = slot;
}

// Take one order out of its level's list. The others keep their places.
// This is the step that no longer depends on how long the queue is.
template <typename Book>
void OrderBook::unlink(Book& book, Slot slot) {
    Order& order = pool_[slot];
    auto level = book.find(order.price);
    Level& queue = level->second;

    if (order.prev != kNoSlot) {
        pool_[order.prev].next = order.next;
    } else {
        queue.head = order.next;  // it was first in line
    }
    if (order.next != kNoSlot) {
        pool_[order.next].prev = order.prev;
    } else {
        queue.tail = order.prev;  // it was last in line
    }

    if (queue.head == kNoSlot) {
        book.erase(level);
    }
}

template <typename Book>
void OrderBook::printSide(const Book& book, const char* name) const {
    for (const auto& [price, queue] : book) {
        out_ << "  " << name << " " << price << ":";
        for (Slot slot = queue.head; slot != kNoSlot; slot = pool_[slot].next) {
            out_ << " " << pool_[slot].id << "x" << pool_[slot].qty;
        }
        out_ << "\n";
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void OrderBook::limit(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                      std::int64_t price, std::optional<std::int64_t> expiry) {
    // The checks run in the order docs/SPEC.md lists them.
    if (id <= 0) {
        reject(seq, "bad_id");
        return;
    }
    if (qty <= 0) {
        reject(seq, "bad_quantity");
        return;
    }
    if (price <= 0) {
        reject(seq, "bad_price");
        return;
    }
    if (expiry && *expiry <= now_) {  // rule 14: the GTT time must be in the future
        reject(seq, "bad_expiry");
        return;
    }
    if (usedIds_.count(id) > 0) {
        reject(seq, "duplicate_id");
        return;
    }
    usedIds_.insert(id);
    enter(seq, side, id, qty, price, expiry);
}

void OrderBook::setTime(std::uint64_t seq, std::int64_t time) {
    if (time < now_) {  // rule 13: the clock never goes back
        reject(seq, "time_backwards");
        return;
    }
    now_ = time;

    // Rule 15: expiries_ is sorted by (GTT time, sequence number), so its front is always the
    // next order to expire. Walking a std::map is fine for determinism: its order is defined by
    // its keys. (Rule 5 in CLAUDE.md is about unordered_map, whose order is not.)
    while (!expiries_.empty() && expiries_.begin()->first.first <= time) {
        Slot slot = expiries_.begin()->second;
        std::int64_t id = pool_[slot].id;
        std::int64_t qty = pool_[slot].qty;
        removeResting(slot);  // also removes it from expiries_
        out_ << "[" << seq << "] EXPIRED id=" << id << " qty=" << qty << "\n";
    }
}

void OrderBook::market(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty) {
    if (id <= 0) {
        reject(seq, "bad_id");
        return;
    }
    if (qty <= 0) {
        reject(seq, "bad_quantity");
        return;
    }
    if (usedIds_.count(id) > 0) {
        reject(seq, "duplicate_id");
        return;
    }
    usedIds_.insert(id);

    // A limit that every price passes: a buy accepts any ask up to the largest possible price;
    // a sell accepts any bid at or above 0, and every bid's price is positive.
    std::int64_t anyPrice = (side == Side::Buy) ? std::numeric_limits<std::int64_t>::max() : 0;
    std::int64_t left = matchIncoming(seq, side, id, qty, anyPrice);

    if (left > 0) {  // rule 8: a market order never rests
        out_ << "[" << seq << "] UNFILLED id=" << id << " qty=" << left << "\n";
    }
}

void OrderBook::modify(std::uint64_t seq, std::int64_t id, std::int64_t qty,
                       std::int64_t price) {
    if (id <= 0) {
        reject(seq, "bad_id");
        return;
    }
    if (qty <= 0) {
        reject(seq, "bad_quantity");
        return;
    }
    if (price <= 0) {
        reject(seq, "bad_price");
        return;
    }

    auto found = index_.find(id);
    if (found == index_.end()) {  // rule 10
        out_ << "[" << seq << "] MODIFY_REJECTED id=" << id << "\n";
        return;
    }
    Slot slot = found->second;
    Order& order = pool_[slot];
    out_ << "[" << seq << "] MODIFIED id=" << id << " qty=" << qty << " price=" << price << "\n";

    if (price == order.price && qty <= order.qty) {
        order.qty = qty;  // rule 11: same price, not more quantity, so it keeps its place
        return;
    }

    // Rule 12: it loses its place and enters again as if it were new, keeping its GTT time
    // (rule 17). Copy what we need first: `order` is reused once its slot is released.
    Side side = order.side;
    std::optional<std::int64_t> expiry = order.expiry;
    removeResting(slot);
    enter(seq, side, id, qty, price, expiry);
}

void OrderBook::cancel(std::uint64_t seq, std::int64_t id) {
    if (id <= 0) {
        reject(seq, "bad_id");
        return;
    }

    auto found = index_.find(id);
    if (found == index_.end()) {  // rule 9
        out_ << "[" << seq << "] CANCEL_REJECTED id=" << id << "\n";
        return;
    }
    removeResting(found->second);
    out_ << "[" << seq << "] CANCELLED id=" << id << "\n";
}

// Asks lowest price first, then bids highest price first. The maps already keep that order.
void OrderBook::print(std::uint64_t seq) const {
    out_ << "[" << seq << "] BOOK\n";
    if (asks_.empty() && bids_.empty()) {
        out_ << "  (empty)\n";
        return;
    }
    printSide(asks_, "ASK");
    printSide(bids_, "BID");
}

void OrderBook::reject(std::uint64_t seq, const char* reason) {
    out_ << "[" << seq << "] REJECTED reason=" << reason << "\n";
}

// ---------------------------------------------------------------------------
// Helpers used by limit, market, modify and cancel
// ---------------------------------------------------------------------------

void OrderBook::enter(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                      std::int64_t price, std::optional<std::int64_t> expiry) {
    std::int64_t left = matchIncoming(seq, side, id, qty, price);
    if (left == 0) {
        return;  // fully filled: it never rests, so it can't expire (rule 14)
    }
    if (side == Side::Buy) {
        rest(bids_, side, seq, id, left, price, expiry);
    } else {
        rest(asks_, side, seq, id, left, price, expiry);
    }
}

void OrderBook::forgetExpiry(const Order& order) {
    if (order.expiry) {
        expiries_.erase({*order.expiry, order.seq});
    }
}

std::int64_t OrderBook::matchIncoming(std::uint64_t seq, Side side, std::int64_t id,
                                      std::int64_t qty, std::int64_t limit) {
    if (side == Side::Buy) {
        return match(asks_, seq, side, id, qty, limit);  // rule 2
    }
    return match(bids_, seq, side, id, qty, limit);      // rule 3
}

void OrderBook::removeResting(Slot slot) {
    std::int64_t id = pool_[slot].id;
    forgetExpiry(pool_[slot]);
    if (pool_[slot].side == Side::Buy) {
        unlink(bids_, slot);
    } else {
        unlink(asks_, slot);
    }
    index_.erase(id);
    release(slot);
}
