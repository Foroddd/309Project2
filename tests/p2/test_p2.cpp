// Assert-based tests for Project 2.
// Everything runs under AddressSanitizer, so memory errors also fail the tests.

#undef NDEBUG
#include "core/conversation.h"
#include "core/message.h"
#include "core/sentinel_scanner.h"
#include "harness/harness.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const std::string kSentinel = "<|end_conversation|>";

//  Helpers ---

// Fake input that returns preset lines, then EOF.
class FakeInput : public InputSource {
public:
    explicit FakeInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}
    std::string read_line() override {
        if (next_ >= lines_.size()) { eof_ = true; return ""; }
        return lines_[next_++];
    }
    bool is_eof() const override { return eof_; }
private:
    std::vector<std::string> lines_;
    std::size_t next_ = 0;
    bool eof_ = false;
};

// Stores everything the harness prints.
class CaptureOutput : public OutputSink {
public:
    void write(std::string_view text) override { text_.append(text); }
    const std::string& text() const { return text_; }
private:
    std::string text_;
};

void write_file(const std::string& path, const std::string& contents) {
    std::ofstream(path) << contents;
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool at_throws(const Conversation& c, std::size_t i) {
    try { (void)c.at(i); } catch (const std::out_of_range&) { return true; }
    return false;
}

bool same_messages(const Conversation& a, const Conversation& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a.at(i).role() != b.at(i).role() || a.at(i).content() != b.at(i).content())
            return false;
    }
    return true;
}

Conversation make_conversation(std::size_t n) {
    Conversation c;
    for (std::size_t i = 0; i < n; ++i)
        c.append(Message(Role::User, "msg " + std::to_string(i)));
    return c;
}

// Feeds text in chunks and returns the output and whether the sentinel was found.
std::pair<std::string, bool> scan_in_chunks(const std::string& text, std::size_t chunk) {
    SentinelScanner s(kSentinel);
    std::string out;
    for (std::size_t i = 0; i < text.size(); i += chunk) {
        auto r = s.feed(std::string_view(text).substr(i, chunk));
        out += r.safe_text;
        if (r.sentinel_found) return {out, true};
    }
    out += s.flush().safe_text;
    return {out, false};
}

// Same format as save_transcript() in main.cpp.
void save_transcript(const Conversation& conv, const std::string& path) {
    std::ofstream file(path);
    const char* names[] = {"system", "user", "assistant"};
    bool first = true;
    for (const Message& m : conv) {
        if (!first) file << "---\n";
        first = false;
        file << "role: " << names[static_cast<int>(m.role())] << "\n" << m.content() << "\n";
    }
}

//  1. Empty conversation bounds 

void test_empty_conversation_bounds() {
    const Conversation c;
    assert(c.size() == 0);
    assert(c.begin() == c.end() && c.begin() == nullptr);
    for (const Message& m : c) { (void)m; assert(false && "no iterations"); }
    assert(at_throws(c, 0));
}

//  2. System message stays first 

void test_system_message_first() {
    Conversation c;
    c.append(Message(Role::System, "Be concise."));
    for (int i = 0; i < 100; ++i) {
        c.append(Message(Role::User, "turn " + std::to_string(i)));
        assert(c.at(0).role() == Role::System && c.at(0).content() == "Be concise.");
    }
    assert(c.size() == 101);
}

//  3. Rule of Five: copy -

void test_copy_is_deep() {
    Conversation original = make_conversation(5);
    Conversation copy(original);
    assert(copy.begin() != original.begin() && "copy must allocate its own buffer");
    assert(same_messages(copy, original));

    original.append(Message(Role::User, "only in original"));
    assert(copy.size() == 5);

    Conversation target = make_conversation(10);
    target = copy;
    assert(target.begin() != copy.begin() && same_messages(target, copy));
}

//  4. Rule of Five: move 

void test_move_steals() {
    Conversation source = make_conversation(5);
    const Message* buffer = source.begin();

    Conversation dest(std::move(source));
    assert(dest.begin() == buffer && "move must steal the pointer");
    assert(dest.size() == 5);
    assert(source.size() == 0 && source.begin() == nullptr);

    Conversation target = make_conversation(7);
    target = std::move(dest);
    assert(target.begin() == buffer && target.size() == 5);
    assert(dest.size() == 0 && dest.begin() == nullptr);
}

//  5. Growth behavior 

// The buffer doubles whenever it becomes full.
void test_growth_doubles() {
    Conversation c;
    for (std::size_t i = 0; i < 1025; ++i) {
        const std::size_t size_before = c.size();
        const Message* before = c.begin();
        c.append(Message(Role::User, "m" + std::to_string(i)));

        const bool expect_realloc = size_before == 0 || (size_before & (size_before - 1)) == 0;
        assert((c.begin() != before) == expect_realloc && "must grow by 2x exactly when full");
        assert(c.size() == size_before + 1);
    }

    for (std::size_t i = 0; i < c.size(); ++i)
        assert(c.at(i).content() == "m" + std::to_string(i));

    assert(at_throws(c, c.size()));
}

//  6. Scanner: clean text 

void test_scanner_clean_text() {
    const std::string text = "plain text with <tags> and <| pipes |> but no stop marker";
    for (std::size_t chunk = 1; chunk <= text.size(); ++chunk) {
        auto [out, found] = scan_in_chunks(text, chunk);
        assert(!found && out == text);
    }
}

//  7. Scanner: sentinel split at every boundary 

void test_scanner_every_boundary() {
    const std::string text = "Goodbye." + kSentinel;
    for (std::size_t split = 0; split <= text.size(); ++split) {
        SentinelScanner scanner(kSentinel);
        auto out1 = scanner.feed(text.substr(0, split));
        auto out2 = scanner.feed(text.substr(split));
        assert((out1.sentinel_found || out2.sentinel_found) &&
               "sentinel must be caught regardless of split point");
        assert(out1.safe_text + out2.safe_text == "Goodbye.");
    }
}

//  8. Scanner: false alarms

void test_scanner_false_alarms() {
    const std::vector<std::string> near_misses = {
        "<|end_world|>", "<|end_conversation|", "<|END_CONVERSATION|>", "<|end_<|end_<|end_",
    };
    for (const std::string& text : near_misses) {
        for (std::size_t chunk = 1; chunk <= text.size(); ++chunk) {
            auto [out, found] = scan_in_chunks(text, chunk);
            assert(!found && "near miss must not trigger");
            assert(out == text && "held-back text must come out on flush()");
        }
    }
}

//  9. Scanner: bounded memory on a 4 MB stream ---

// Keeps the scanner holding the maximum amount of text.
void test_scanner_bounded_memory() {
    const std::string pattern = kSentinel.substr(0, kSentinel.size() - 1);
    const std::size_t bound = kSentinel.size() - 1;

    SentinelScanner s(kSentinel);
    std::size_t fed = 0, emitted = 0;
    while (fed < 4u * 1024u * 1024u) {
        for (char ch : pattern) {
            auto r = s.feed(std::string_view(&ch, 1));
            assert(!r.sentinel_found);
            ++fed;
            emitted += r.safe_text.size();
            assert(fed - emitted <= bound && "pending_ exceeded sentinel.size() - 1");
        }
    }

    emitted += s.flush().safe_text.size();
    assert(emitted == fed && "every byte comes out exactly once");
}

//  10. Harness: turn limit ---

void test_harness_turn_limit() {
    const std::string path = "test_turn_limit.script";
    write_file(path, "role: assistant\none\n---\nrole: assistant\ntwo\n---\n"
                     "role: assistant\nthree\n---\nrole: assistant\nfour\n");

    HarnessConfig cfg;
    cfg.max_turns = 3;
    Harness h(std::make_unique<ScriptedModelClient>(path), cfg);
    FakeInput in({"hi", "hi", "hi", "hi"});
    CaptureOutput out;

    StopReason reason = h.run(in, out);
    assert(reason.kind == StopReason::Kind::TurnLimit);
    assert(h.conversation().size() == 6);
    std::remove(path.c_str());
}

//  11. Harness: halts on a split sentinel 

void test_harness_sentinel_halt() {
    const std::string path = "test_sentinel.script";
    write_file(path, "chunk: 3\nrole: assistant\nHi there!\n---\n"
                     "chunk: 4\nrole: assistant\nGoodbye.<|end_conversation|>\n---\n"
                     "role: assistant\nnever requested\n");

    HarnessConfig cfg;
    cfg.max_turns = 10;
    Harness h(std::make_unique<ScriptedModelClient>(path), cfg);
    FakeInput in({"hello", "bye", "never read"});
    CaptureOutput out;

    StopReason reason = h.run(in, out);
    assert(reason.kind == StopReason::Kind::Sentinel);
    assert(reason.detail == "stop sentinel after 2 turns");
    assert(out.text().find("<|") == std::string::npos);
    assert(out.text().find("Goodbye.") != std::string::npos);
    assert(h.conversation().at(3).content() == "Goodbye." + kSentinel);
    std::remove(path.c_str());
}

//  12. Transcript round-trip -

void test_transcript_round_trip() {
    const std::string script = "test_roundtrip.script";
    const std::string saved1 = "test_roundtrip1.txt";
    const std::string saved2 = "test_roundtrip2.txt";

    write_file(script, "role: system\nBe concise.\n---\n"
                       "chunk: 5\nrole: assistant\nI am doing well. How can I help?\n---\n"
                       "chunk: 6\nrole: assistant\nGoodbye!<|end_conversation|>\n");

    const std::vector<std::string> inputs = {"hello", "bye"};

    // Run the original session and save it.
    auto scripted = std::make_unique<ScriptedModelClient>(script);
    HarnessConfig cfg1;
    cfg1.system_message = scripted->system_message();
    Harness live(std::move(scripted), cfg1);
    FakeInput in1(inputs);
    CaptureOutput out1;
    StopReason r1 = live.run(in1, out1);
    save_transcript(live.conversation(), saved1);

    // Replay the saved session.
    auto replay = std::make_unique<ReplayModelClient>(saved1);
    HarnessConfig cfg2;
    cfg2.system_message = replay->system_message();
    Harness replayed(std::move(replay), cfg2);
    FakeInput in2(inputs);
    CaptureOutput out2;
    StopReason r2 = replayed.run(in2, out2);
    save_transcript(replayed.conversation(), saved2);

    assert(r2.kind == r1.kind && r2.detail == r1.detail);
    assert(out2.text() == out1.text());
    assert(same_messages(replayed.conversation(), live.conversation()));
    assert(read_file(saved2) == read_file(saved1));

    std::remove(script.c_str());
    std::remove(saved1.c_str());
    std::remove(saved2.c_str());
}

//  13. Harness: EOF --

void test_harness_eof() {
    const std::string path = "test_eof.script";
    write_file(path, "role: assistant\nfirst\n---\nrole: assistant\nsecond\n");

    Harness h(std::make_unique<ScriptedModelClient>(path), HarnessConfig{});
    FakeInput in({"one line"});
    CaptureOutput out;

    StopReason reason = h.run(in, out);
    assert(reason.kind == StopReason::Kind::UserExit);
    assert(h.conversation().size() == 2);
    std::remove(path.c_str());
}

}  // namespace

int main() {
    const std::pair<const char*, void (*)()> tests[] = {
        {"1  empty conversation bounds", test_empty_conversation_bounds},
        {"2  system message first", test_system_message_first},
        {"3  copy is deep", test_copy_is_deep},
        {"4  move steals", test_move_steals},
        {"5  growth doubles", test_growth_doubles},
        {"6  scanner clean text", test_scanner_clean_text},
        {"7  scanner every boundary", test_scanner_every_boundary},
        {"8  scanner false alarms", test_scanner_false_alarms},
        {"9  scanner bounded memory", test_scanner_bounded_memory},
        {"10 harness turn limit", test_harness_turn_limit},
        {"11 harness sentinel halt", test_harness_sentinel_halt},
        {"12 transcript round-trip", test_transcript_round_trip},
        {"13 harness EOF", test_harness_eof},
    };

    for (const auto& [name, fn] : tests) {
        fn();
        std::cout << "[OK] " << name << "\n";
    }

    std::cout << "All 13 tests passed.\n";
    return 0;
}