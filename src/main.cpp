// Matching engine v0: a limit order book with price-time priority.
// Reads a command file (format in docs/SPEC.md) and prints trades, cancels and the book.

#include <algorithm>      // std::min, std::find_if
#include <cstdint>        // std::int64_t, std::uint64_t
#include <deque>
#include <fstream>
#include <functional>     // std::greater
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>

enum class Side { Buy, Sell };

// One order resting in the book.
struct Order {
    std::uint64_t id;
    std::int64_t qty;   // quantity still left
    std::uint64_t seq;  // sequence number of the command that placed it (for debugging)
};

// All orders at one price, in arrival order. The front order trades first (rule 4).
using Level = std::deque<Order>;

// Where a resting order sits, so CANCEL can find it without searching the whole book.
struct Location {
    Side side;
    std::int64_t price;
};

class OrderBook {
public:
    explicit OrderBook(std::ostream& out) : out_(out) {}

    // Rule 2: a BUY trades with asks at or below its price; whatever is left rests (rule 8).
    void buy(std::uint64_t seq, std::uint64_t id, std::int64_t qty, std::int64_t price) {
        qty = match(asks_, seq, id, Side::Buy, qty, price);
        if (qty > 0) {
            rest(bids_, Side::Buy, seq, id, qty, price);
        }
    }

    // Rule 3: a SELL trades with bids at or above its price; whatever is left rests (rule 8).
    void sell(std::uint64_t seq, std::uint64_t id, std::int64_t qty, std::int64_t price) {
        qty = match(bids_, seq, id, Side::Sell, qty, price);
        if (qty > 0) {
            rest(asks_, Side::Sell, seq, id, qty, price);
        }
    }

    // Rule 9: remove a resting order, or reject if it isn't resting.
    void cancel(std::uint64_t seq, std::uint64_t id) {
        auto found = index_.find(id);  // look up by key only, never loop (CLAUDE.md rule 5)
        if (found == index_.end()) {
            out_ << "[" << seq << "] CANCEL_REJECTED id=" << id << "\n";
            return;
        }
        Location where = found->second;
        index_.erase(found);
        if (where.side == Side::Buy) {
            removeOrder(bids_, where.price, id);
        } else {
            removeOrder(asks_, where.price, id);
        }
        out_ << "[" << seq << "] CANCELLED id=" << id << "\n";
    }

    // Asks lowest price first, then bids highest price first. The maps already keep that order.
    void print(std::uint64_t seq) const {
        out_ << "[" << seq << "] BOOK\n";
        if (asks_.empty() && bids_.empty()) {
            out_ << "  (empty)\n";
            return;
        }
        printSide(asks_, "ASK");
        printSide(bids_, "BID");
    }

private:
    // Trade the incoming order against the other side of the book.
    // Returns the quantity the incoming order still has left.
    // `Book` is either the bids map or the asks map; the code is the same for both.
    template <typename Book>
    std::int64_t match(Book& opposite, std::uint64_t seq, std::uint64_t id, Side side,
                       std::int64_t qty, std::int64_t limit) {
        while (qty > 0 && !opposite.empty()) {
            auto level = opposite.begin();  // the best price on the other side
            std::int64_t levelPrice = level->first;

            bool crosses = (side == Side::Buy) ? (levelPrice <= limit) : (levelPrice >= limit);
            if (!crosses) {
                break;
            }

            Level& queue = level->second;
            Order& resting = queue.front();                       // earliest order at this price
            std::int64_t traded = std::min(qty, resting.qty);     // rule 5

            std::uint64_t buyId = (side == Side::Buy) ? id : resting.id;
            std::uint64_t sellId = (side == Side::Buy) ? resting.id : id;
            out_ << "[" << seq << "] TRADE buy=" << buyId << " sell=" << sellId
                 << " qty=" << traded << " price=" << levelPrice << "\n";  // rule 6

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
    void rest(Book& book, Side side, std::uint64_t seq, std::uint64_t id, std::int64_t qty,
              std::int64_t price) {
        book[price].push_back(Order{id, qty, seq});  // [] creates the level if it's new
        index_[id] = Location{side, price};
    }

    // Remove one order from its level. The other orders keep their places.
    template <typename Book>
    static void removeOrder(Book& book, std::int64_t price, std::uint64_t id) {
        auto level = book.find(price);
        Level& queue = level->second;
        auto it = std::find_if(queue.begin(), queue.end(),
                               [id](const Order& order) { return order.id == id; });
        queue.erase(it);
        if (queue.empty()) {
            book.erase(level);
        }
    }

    template <typename Book>
    void printSide(const Book& book, const char* name) const {
        for (const auto& [price, queue] : book) {
            out_ << "  " << name << " " << price << ":";
            for (const Order& order : queue) {
                out_ << " " << order.id << "x" << order.qty;
            }
            out_ << "\n";
        }
    }

    std::map<std::int64_t, Level, std::greater<std::int64_t>> bids_;  // highest price first
    std::map<std::int64_t, Level> asks_;                              // lowest price first
    std::unordered_map<std::uint64_t, Location> index_;  // order id -> where it rests
    std::ostream& out_;
};

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

    while (std::getline(file, line)) {
        // Blank lines and comments get no sequence number.
        if (line.empty() || line[0] == '#') {
            continue;
        }
        ++seq;

        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "BUY" || cmd == "SELL") {
            std::uint64_t id = 0;
            std::int64_t qty = 0;
            std::int64_t price = 0;
            in >> id >> qty >> price;
            if (cmd == "BUY") {
                book.buy(seq, id, qty, price);
            } else {
                book.sell(seq, id, qty, price);
            }
        } else if (cmd == "CANCEL") {
            std::uint64_t id = 0;
            in >> id;
            book.cancel(seq, id);
        } else if (cmd == "PRINT") {
            book.print(seq);
        } else {
            // v0 assumes valid input; v1 will define how to reject bad commands.
            std::cerr << "[" << seq << "] unknown command: " << cmd << "\n";
        }
    }

    return 0;
}
