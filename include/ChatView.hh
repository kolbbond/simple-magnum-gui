// chat transcript widget: ChatModel is pure data (usable without a GL context),
// ChatView draws it inside the current ImGui window with an input box below.
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"

namespace smg {

// collapsible child of a message (e.g. a tool call)
struct ChatItem {
    std::string title;
    std::string body;
    bool open = false;
    bool error = false;
};

struct ChatMessage {
    std::string role;
    std::string text;
    std::vector<ChatItem> items;
    bool divider = false; // drawn as a separator carrying `text`
};

class ChatModel {
public:
    size_t add(ChatMessage m); // returns the message index
    // index-taking calls throw std::out_of_range on a bad index
    void append_text(size_t idx, std::string_view delta);
    size_t add_item(size_t idx, ChatItem item); // returns the item index
    ChatItem& item(size_t msg, size_t item);
    const std::vector<ChatMessage>& messages() const { return _msgs; }
    void clear() { _msgs.clear(); }

private:
    std::vector<ChatMessage> _msgs;
};

class ChatView {
public:
    // font for fenced code lines; nullptr = the current font
    void set_code_font(ImFont* font) { _code_font = font; }
    ChatModel& model() { return _model; }
    void set_role_color(const std::string& role, ImVec4 color) { _colors[role] = color; }

    // Draws inside the current ImGui window. Returns the submitted text (Enter;
    // Shift+Enter = newline) or nullopt. Whitespace-only input is not submitted.
    std::optional<std::string> draw(bool input_enabled = true);

private:
    ChatModel _model;
    std::map<std::string, ImVec4> _colors;
    std::string _input;
    bool _refocus = false;
    ImFont* _code_font = nullptr;
};

} // namespace smg
