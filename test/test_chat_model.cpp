// unit test: ChatModel is pure data (no GL context)
#include "ChatView.hh"
#include "test_util.hh"

#include <stdexcept>

using namespace smg;

template <class F>
static bool throws_oor(F&& f) {
    try {
        f();
    } catch(const std::out_of_range&) { return true; }
    return false;
}

static void append_text_accumulates() {
    ChatModel m;
    const size_t i = m.add({ "assistant", "Hel", {}, false });
    m.append_text(i, "lo ");
    m.append_text(i, "world");
    CHECK(i == 0);
    CHECK(m.messages()[0].text == "Hello world");
    CHECK(m.messages()[0].role == "assistant");
}

static void items_attach_to_message() {
    ChatModel m;
    m.add({ "user", "hi", {}, false });
    const size_t b = m.add({ "assistant", "", {}, false });
    CHECK(m.add_item(b, { "tool", "args", false, false }) == 0);
    CHECK(m.add_item(b, { "tool2", "x", false, true }) == 1);
    CHECK(m.messages()[0].items.empty());
    CHECK(m.messages()[1].items.size() == 2);
    m.item(b, 1).open = true;
    CHECK(m.messages()[1].items[1].open);
    CHECK(m.messages()[1].items[1].error);
    m.item(b, 0).body += "+more";
    CHECK(m.messages()[1].items[0].body == "args+more");
}

static void divider_message_flag() {
    ChatModel m;
    m.add({ "", "switched to agent B", {}, true });
    m.add({ "user", "x", {}, false });
    CHECK(m.messages()[0].divider);
    CHECK(!m.messages()[1].divider);
}

static void clear_resets() {
    ChatModel m;
    const size_t i = m.add({ "user", "a", {}, false });
    m.add_item(i, { "t", "b", false, false });
    m.clear();
    CHECK(m.messages().empty());
    CHECK(m.add({ "user", "b", {}, false }) == 0);
}

static void out_of_range_throws() {
    ChatModel m;
    CHECK(throws_oor([&] { m.append_text(0, "x"); }));
    CHECK(throws_oor([&] { m.add_item(0, {}); }));
    CHECK(throws_oor([&] { m.item(0, 0); }));
    const size_t i = m.add({ "user", "a", {}, false });
    CHECK(throws_oor([&] { m.item(i, 0); }));
    CHECK(throws_oor([&] { m.append_text(i + 1, "x"); }));
}

int main() {
    append_text_accumulates();
    items_attach_to_message();
    divider_message_flag();
    clear_resets();
    out_of_range_throws();
    TEST_RETURN();
}
