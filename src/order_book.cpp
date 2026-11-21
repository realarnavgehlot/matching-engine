#include "order_book.hpp"

#include <algorithm>  // std::min, std::find_if
#include <limits>     // std::numeric_limits

namespace {

// Find order `id` in one level's queue. The caller knows it is there (the index says so).
Level::iterator findOrder(Level& queue, std::int64_t id) {
    return std::find_if(queue.begin(), queue.end(),
                        [id](const Order& order) { return order.id == id; });
}

}  // namespace

OrderBook::OrderBook(std::ostream& out) : out_(out) {}

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
        Order& resting = queue.front();                    // earliest order at this price
        std::int64_t traded = std::min(qty, resting.qty);  // rule 5

        std::int64_t buyId = (side == Side::Buy) ? id : resting.id;
        std::int64_t sellId = (side == Side::Buy) ? resting.id : id;
        out_ << "[" << seq << "] TRADE buy=" << buyId << " sell=" << sellId << " qty=" << traded
             << " price=" << levelPrice << "\n";  // rule 6

        qty -= traded;
        resting.qty -= traded;

        // Rule 7: a filled order leaves the book, and so does an empty level.
        if (resting.qty == 0) {
            index_.erase(resting.id);  // before pop_front(), while `resting` still exists
            queue.pop_front();
        }
        if (queue.empty()) {
            opposite.erase(level);
        }
    }
    return qty;
}

// Put an order at the back of the queue at its price (rule 8).
template <typename Book>
void OrderBook::rest(Book& book, Side side, std::uint64_t seq, std::int64_t id, std::int64_t qty,
                     std::int64_t price) {
    book[price].push_back(Order{id, qty, seq});  // [] creates the level if it's new
    index_[id] = Location{side, price};
}

// Take one order out of its level. The other orders keep their places.
template <typename Book>
void OrderBook::removeOrder(Book& book, std::int64_t price, std::int64_t id) {
    auto level = book.find(price);
    Level& queue = level->second;
    queue.erase(findOrder(queue, id));
    if (queue.empty()) {
        book.erase(level);
    }
    index_.erase(id);
}

// Rules 11 and 12.
template <typename Book>
void OrderBook::modifyIn(Book& book, std::uint64_t seq, Side side, std::int64_t id,
                         std::int64_t qty, std::int64_t price, std::int64_t oldPrice) {
    out_ << "[" << seq << "] MODIFIED id=" << id << " qty=" << qty << " price=" << price << "\n";

    Level& queue = book.find(oldPrice)->second;
    auto order = findOrder(queue, id);

    if (price == oldPrice && qty <= order->qty) {
        order->qty = qty;  // rule 11: same price, not more quantity, so it keeps its place
        return;
    }

    // Rule 12: it loses its place and enters again as if it were new.
    removeOrder(book, oldPrice, id);
    enter(seq, side, id, qty, price);
}

template <typename Book>
void OrderBook::printSide(const Book& book, const char* name) const {
    for (const auto& [price, queue] : book) {
        out_ << "  " << name << " " << price << ":";
        for (const Order& order : queue) {
            out_ << " " << order.id << "x" << order.qty;
        }
        out_ << "\n";
    }
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void OrderBook::limit(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                      std::int64_t price) {
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
    if (usedIds_.count(id) > 0) {
        reject(seq, "duplicate_id");
        return;
    }
    usedIds_.insert(id);
    enter(seq, side, id, qty, price);
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
    Location where = found->second;
    if (where.side == Side::Buy) {
        modifyIn(bids_, seq, where.side, id, qty, price, where.price);
    } else {
        modifyIn(asks_, seq, where.side, id, qty, price, where.price);
    }
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
    Location where = found->second;
    if (where.side == Side::Buy) {
        removeOrder(bids_, where.price, id);
    } else {
        removeOrder(asks_, where.price, id);
    }
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
// Helpers used by limit, market and modify
// ---------------------------------------------------------------------------

void OrderBook::enter(std::uint64_t seq, Side side, std::int64_t id, std::int64_t qty,
                      std::int64_t price) {
    std::int64_t left = matchIncoming(seq, side, id, qty, price);
    if (left == 0) {
        return;
    }
    if (side == Side::Buy) {
        rest(bids_, side, seq, id, left, price);
    } else {
        rest(asks_, side, seq, id, left, price);
    }
}

std::int64_t OrderBook::matchIncoming(std::uint64_t seq, Side side, std::int64_t id,
                                      std::int64_t qty, std::int64_t limit) {
    if (side == Side::Buy) {
        return match(asks_, seq, side, id, qty, limit);  // rule 2
    }
    return match(bids_, seq, side, id, qty, limit);      // rule 3
}
