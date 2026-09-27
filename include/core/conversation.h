// Stores a growable list of Messages, oldest first.
// This is the only class that uses new[] / delete[].

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Starts empty with no allocated memory
    Conversation() = default;

    // Frees the allocated memory
    ~Conversation();

    // Makes a deep copy of another Conversation
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Moves the data form another Conversation.
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Adds a message, doubling the array if needed
    void append(Message m);

    // Returns the number of messages.
    std::size_t size() const noexcept;

    // Returns the message at index i. Throws if i is out of bounds
    const Message& at(std::size_t i) const;

    // Allows range-for loops through the messages
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    // Swaps the data between two Conversations
    void swap(Conversation& other) noexcept;

    Message*    data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};