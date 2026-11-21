// A fast way to write the engine's output lines (v2, Step 16).
//
// Writing with std::ostream's << is slow for two reasons: every number goes through the
// locale machinery to be formatted, and every small piece is handed to the stream on its own.
// Output instead collects text in a 64 KB buffer, formats numbers with std::to_chars, and hands
// the stream one big block at a time. The bytes written are exactly the same.

#pragma once

#include <charconv>  // std::to_chars
#include <cstdint>
#include <cstring>   // std::strlen, std::memcpy
#include <ostream>
#include <vector>

class Output {
public:
    explicit Output(std::ostream& sink) : sink_(sink), buffer_(kSize) {}
    ~Output() { flush(); }

    // One Output per stream: copying it would write the same buffered text twice.
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;

    Output& operator<<(const char* text) {
        append(text, std::strlen(text));
        return *this;
    }
    Output& operator<<(std::int64_t value) {
        number(value);
        return *this;
    }
    Output& operator<<(std::uint64_t value) {
        number(value);
        return *this;
    }

    // Hand everything collected so far to the stream.
    void flush() {
        if (used_ > 0) {
            sink_.write(buffer_.data(), static_cast<std::streamsize>(used_));
            used_ = 0;
        }
    }

private:
    static constexpr std::size_t kSize = 1 << 16;  // 64 KB
    static constexpr std::size_t kMaxNumber = 20;  // characters in the longest 64-bit number

    void append(const char* text, std::size_t length) {
        if (used_ + length > kSize) {
            flush();
        }
        if (length > kSize) {  // too big for the buffer: write it straight through
            sink_.write(text, static_cast<std::streamsize>(length));
            return;
        }
        std::memcpy(buffer_.data() + used_, text, length);
        used_ += length;
    }

    template <typename Integer>
    void number(Integer value) {
        if (used_ + kMaxNumber > kSize) {
            flush();
        }
        char* first = buffer_.data() + used_;
        char* last = buffer_.data() + kSize;
        auto result = std::to_chars(first, last, value);  // always fits: we made room above
        used_ = static_cast<std::size_t>(result.ptr - buffer_.data());
    }

    std::ostream& sink_;
    std::vector<char> buffer_;
    std::size_t used_ = 0;
};
