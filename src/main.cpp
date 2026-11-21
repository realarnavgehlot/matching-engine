// Matching engine: reads a command file and runs each command on the order book.
// The input format and every output line are defined in docs/SPEC.md.

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>     // std::istreambuf_iterator
#include <string>
#include <string_view>

#include "command.hpp"
#include "order_book.hpp"

namespace {

// Read the whole file into memory in one go (v4, Step 23). One big read is much faster than
// a std::getline call, and a std::string copy, for every line.
// Binary mode: no line-ending conversion, so a Windows file keeps its '\r' before each '\n'.
// That's fine: the parser treats '\r' as a space, exactly as before.
bool readWholeFile(const char* path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();  // -1 if the size can't be found
    file.seekg(0, std::ios::beg);
    if (size > 0) {
        text.resize(static_cast<std::size_t>(size));
        file.read(text.data(), size);
        text.resize(static_cast<std::size_t>(file.gcount()));
    } else {
        // Size unknown (or zero): read it piece by piece instead.
        text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    // The command file must be given: .\engine.exe data\sample.txt
    if (argc < 2) {
        std::cerr << "usage: engine.exe <command file>\n";
        return 1;
    }

    std::string text;
    if (!readWholeFile(argv[1], text)) {
        std::cerr << "error: could not open " << argv[1] << "\n";
        return 1;
    }

    // Guess how many orders the file holds from its size (a command line is about 16-20 bytes),
    // so the book can size its hash tables once. The guess affects speed only, never output.
    OrderBook book(std::cout, text.size() / 16);
    std::uint64_t seq = 0;  // sequence number of the current command

    // Walk the text one line at a time. Each line is a std::string_view into `text`: no copies.
    // This gives the same lines std::getline would: the text between '\n's, and a last line
    // without a '\n' if the file doesn't end with one.
    std::string_view all(text);
    std::size_t start = 0;
    while (start < all.size()) {
        std::size_t end = all.find('\n', start);
        if (end == std::string_view::npos) {
            end = all.size();
        }
        std::string_view line = all.substr(start, end - start);
        start = end + 1;

        if (!isCommandLine(line)) {
            continue;  // blank or comment: no sequence number
        }
        ++seq;  // every other line is a command, even an invalid one
        runCommand(book, seq, parseCommand(line));
    }

    book.flush();  // write out whatever is still in the output buffer
    return 0;
}
