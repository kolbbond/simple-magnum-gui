#include "ChatView.hh"

#include <algorithm>
#include <cfloat>
#include <stdexcept>

namespace smg {

size_t ChatModel::add(ChatMessage m) {
    _msgs.push_back(std::move(m));
    return _msgs.size() - 1;
}

void ChatModel::append_text(size_t idx, std::string_view delta) {
    if(idx >= _msgs.size()) throw std::out_of_range("ChatModel::append_text: bad message index");
    _msgs[idx].text.append(delta);
}

size_t ChatModel::add_item(size_t idx, ChatItem item) {
    if(idx >= _msgs.size()) throw std::out_of_range("ChatModel::add_item: bad message index");
    _msgs[idx].items.push_back(std::move(item));
    return _msgs[idx].items.size() - 1;
}

ChatItem& ChatModel::item(size_t msg, size_t item) {
    if(msg >= _msgs.size() || item >= _msgs[msg].items.size()) throw std::out_of_range("ChatModel::item: bad index");
    return _msgs[msg].items[item];
}

namespace {

const ImVec4 kCodeColor(0.75f, 0.85f, 0.65f, 1.f);
const ImVec4 kErrorColor(1.f, 0.35f, 0.35f, 1.f);

// wrapped text with ``` fences rendered as indented, tinted lines (the default font is already monospace)
void draw_text(const std::string& text, const ImVec4* color) {
    if(color != nullptr) ImGui::PushStyleColor(ImGuiCol_Text, *color);
    bool code = false;
    size_t pos = 0;
    while(pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if(end == std::string::npos) end = text.size();
        const std::string_view line(text.data() + pos, end - pos);
        if(line.rfind("```", 0) == 0) {
            code = !code;
        } else if(code) {
            ImGui::Indent();
            ImGui::PushStyleColor(ImGuiCol_Text, kCodeColor);
            ImGui::TextUnformatted(line.data(), line.data() + line.size());
            ImGui::PopStyleColor();
            ImGui::Unindent();
        } else {
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted(line.data(), line.data() + line.size());
            ImGui::PopTextWrapPos();
        }
        pos = end + 1;
    }
    if(color != nullptr) ImGui::PopStyleColor();
}

// std::string-backed InputText (imgui_stdlib is not built into smg)
int resize_cb(ImGuiInputTextCallbackData* d) {
    if(d->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        auto* s = static_cast<std::string*>(d->UserData);
        s->resize(static_cast<size_t>(d->BufTextLen));
        d->Buf = s->data();
    }
    return 0;
}

} // namespace

std::optional<std::string> ChatView::draw(bool input_enabled) {
    const float input_h = ImGui::GetTextLineHeightWithSpacing() * 3 + ImGui::GetStyle().FramePadding.y * 2;
    const float log_h = std::max(1.f, ImGui::GetContentRegionAvail().y - input_h - ImGui::GetStyle().ItemSpacing.y);

    ImGui::BeginChild("##chat_log", ImVec2(0, log_h), ImGuiChildFlags_None);
    // stick to the bottom unless the user scrolled up
    const bool stick = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.f;

    auto& msgs = _model.messages();
    for(size_t mi = 0; mi < msgs.size(); ++mi) {
        const ChatMessage& m = msgs[mi];
        ImGui::PushID(static_cast<int>(mi));
        if(m.divider) {
            ImGui::SeparatorText(m.text.c_str());
        } else {
            const auto it = _colors.find(m.role);
            const ImVec4* col = it == _colors.end() ? nullptr : &it->second;
            if(!m.role.empty()) {
                if(col != nullptr) ImGui::PushStyleColor(ImGuiCol_Text, *col);
                ImGui::TextUnformatted(m.role.c_str());
                if(col != nullptr) ImGui::PopStyleColor();
            }
            draw_text(m.text, col);
            for(size_t ii = 0; ii < m.items.size(); ++ii) {
                ChatItem& item = _model.item(mi, ii);
                ImGui::PushID(static_cast<int>(ii));
                if(item.error) ImGui::PushStyleColor(ImGuiCol_Text, kErrorColor);
                ImGui::SetNextItemOpen(item.open, ImGuiCond_Always);
                item.open = ImGui::TreeNodeEx(item.title.c_str(), ImGuiTreeNodeFlags_None);
                if(item.error) ImGui::PopStyleColor();
                if(item.open) {
                    draw_text(item.body, item.error ? &kErrorColor : nullptr);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
        ImGui::PopID();
    }
    if(stick) ImGui::SetScrollHereY(1.f);
    ImGui::EndChild();

    std::optional<std::string> out;
    ImGuiIO& io = ImGui::GetIO();
    if(!input_enabled) ImGui::BeginDisabled();
    if(_refocus) {
        ImGui::SetKeyboardFocusHere();
        _refocus = false;
    }
    // ImGui validates on Ctrl+Enter when CtrlEnterForNewLine is set; present Shift as Ctrl so Shift+Enter inserts the newline
    const bool ctrl = io.KeyCtrl;
    io.KeyCtrl = io.KeyShift;
    _input.reserve(256); // InputText needs a non-null, writable buffer
    const bool entered = ImGui::InputTextMultiline("##chat_input",
        _input.data(),
        _input.capacity() + 1,
        ImVec2(-FLT_MIN, input_h),
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_CallbackResize,
        resize_cb,
        &_input);
    io.KeyCtrl = ctrl;
    if(!input_enabled) ImGui::EndDisabled();
    if(entered) {
        const bool blank = _input.find_first_not_of(" \t\r\n") == std::string::npos;
        if(!blank) {
            out = std::move(_input);
            _input.clear();
        }
        _refocus = true; // Enter deactivates the widget
    }
    return out;
}

} // namespace smg
