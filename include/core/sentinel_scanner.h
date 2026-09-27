// Scans incoming text for a multi-character stop sequence
// Holds back characters that could still be part of the sequence.

#pragma once
#include <string>
#include <string_view>

class SentinelScanner {
public:
    explicit SentinelScanner(std::string sentinel);

    struct Out { std::string safe_text; bool sentinel_found; };

    // Processes the next chunk of text and returns safe text
    // If the sentinel is found, everything afters ignored.
    Out feed(std::string_view chunk);

    // Releases any text still being held when the stream ends
    Out flush();

private:
    std::string sentinel_;
    std::string pending_;   // stores characters that could be part of the sentinel
};