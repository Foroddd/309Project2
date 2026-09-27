#include "core/conversation.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr std::size_t kGrowthFactor = 2;
}  // namespace

Conversation::~Conversation() {
    delete[] data_;  // delete[] on nullptr is a no-op (moved-from / empty)
}

// Copy

Conversation::Conversation(const Conversation& other) {
    if (other.size_ == 0) return;  // copying an empty conversation allocates nothing

    // Allocate first. If new[] throws, no member has been touched yet and
    // the half-built object owns nothing, so nothing leaks.
    Message* fresh = new Message[other.size_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];  // std::string copy may throw bad_alloc
        }
    } catch (...) {
        delete[] fresh;
        throw;
    }
    data_ = fresh;
    size_ = other.size_;
    capacity_ = other.size_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    // Copies and swaps.
    // tmp's destructor frees our old buffer
    Conversation tmp(other);
    swap(tmp);
    return *this;
}

// move

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    // Steal the pointer, then zero the source so its destructor frees nothing.
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;  // release what we owned before

        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void Conversation::swap(Conversation& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

// growth

void Conversation::append(Message m) {
    // m is taken by value, so even conv.append(conv.at(0)) is safe: the
    // copy is made before any reallocation can invalidate at(0).
    if (size_ == capacity_) {
        const std::size_t new_capacity =
            (capacity_ == 0) ? 1 : capacity_ * kGrowthFactor;

        Message* bigger = new Message[new_capacity];  // may throw; nothing changed yet

        // Message's move assignment is noexcept (enum + std::string), so
        // this loop cant fail halfway.
        for (std::size_t i = 0; i < size_; ++i) {
            bigger[i] = std::move(data_[i]);
        }

        delete[] data_;
        data_ = bigger;
        capacity_ = new_capacity;
    }

    data_[size_] = std::move(m);
    ++size_;
}

// Access

std::size_t Conversation::size() const noexcept {
    return size_;
}

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index " + std::to_string(i) +
                                " out of range (size " + std::to_string(size_) + ")");
    }
    return data_[i];
}

const Message* Conversation::begin() const noexcept {
    return data_;
}

const Message* Conversation::end() const noexcept {
    // When empty, data_ is nullptr and size_ is 0, so this is nullptr too.
    return data_ + size_;
}
