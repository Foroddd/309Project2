#include "core/sentinel_scanner.h"
#include <algorithm>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    // An empty sentinel means theres nothing to look for.
    if (sentinel_.empty()) {
        return {std::string(chunk), false};
    }

    // Combine the saved characters with the new chunk
    std::string window;
    window.reserve(pending_.size() + chunk.size());
    window += pending_;
    window.append(chunk.data(), chunk.size());
    pending_.clear();

    // If the sentinel is found, return everything before it
    const std::size_t pos = window.find(sentinel_);
    if (pos != std::string::npos) {
        window.resize(pos);
        return {std::move(window), true};
    }

    // Hold back the longest ending part that could still become the sentinel.
    const std::size_t max_hold = std::min(window.size(), sentinel_.size() - 1);
    std::size_t hold = 0;
    for (std::size_t len = max_hold; len > 0; --len) {
        if (window.compare(window.size() - len, len, sentinel_, 0, len) == 0) {
            hold = len;
            break;
        }
    }

    pending_.assign(window, window.size() - hold, hold);
    window.resize(window.size() - hold);
    return {std::move(window), false};
}

SentinelScanner::Out SentinelScanner::flush() {
    // The stream ended, so any remaining characters are normal text.
    Out out{std::move(pending_), false};
    pending_.clear();
    return out;
}