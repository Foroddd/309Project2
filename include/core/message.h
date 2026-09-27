// Stores one message with a role and its content.
// Header-only, so there is no separate .cpp file.

#pragma once
#include <string>
#include <utility>

enum class Role { System, User, Assistant };

class Message {
public:
    // Creates an empty System message.
    Message() : role_(Role::System), content_() {}

    // Creates a message with the given role and content.
    Message(Role role, std::string content)
        : role_(role), content_(std::move(content)) {}

    Role               role()    const noexcept { return role_; }
    const std::string& content() const noexcept { return content_; }

private:
    Role        role_;
    std::string content_;
};