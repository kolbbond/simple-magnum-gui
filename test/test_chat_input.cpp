// unit test: ChatView::draw under a bare ImGui context (no window, no GL)
#include "ChatView.hh"
#include "imgui.h"
#include "test_util.hh"

using namespace smg;

namespace {
struct Harness {
    ChatView view;
    std::optional<std::string> last;
    bool enabled = true;
    bool collapsed = false;

    void frame() {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = { 800, 600 };
        io.DeltaTime = 1.f / 60.f;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({ 0, 0 });
        ImGui::SetNextWindowSize({ 800, 600 });
        if(collapsed) ImGui::SetNextWindowCollapsed(true, ImGuiCond_Always);
        ImGui::Begin("c");
        if(auto r = view.draw(enabled)) last = r;
        ImGui::End();
        ImGui::Render();
    }
    void key(ImGuiKey k, bool down) { ImGui::GetIO().AddKeyEvent(k, down); }
    void tap(ImGuiKey k) {
        key(k, true);
        frame();
        key(k, false);
        frame();
    }
    void chr(unsigned c) {
        ImGui::GetIO().AddInputCharacter(c);
        frame();
    }
    void shift(bool d) {
        key(ImGuiMod_Shift, d);
        key(ImGuiKey_LeftShift, d);
        frame();
    }
    void click_input() {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(400, 580);
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
    }
    void take() { last.reset(); }
};
} // namespace

int main() {
    ImGui::CreateContext();
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetIO().IniFilename = nullptr; // never persist window state between runs

    {
        Harness h;
        for(int i = 0; i < 3; ++i) h.frame();
        h.click_input();
        h.chr('a');
        h.shift(true);
        h.chr('B'); // shifted chars must still be typed
        h.tap(ImGuiKey_Enter);
        h.shift(false);
        h.chr('c');
        h.key(ImGuiMod_Ctrl, true);
        h.key(ImGuiKey_LeftCtrl, true);
        h.frame();
        h.tap(ImGuiKey_Enter);
        h.key(ImGuiMod_Ctrl, false);
        h.key(ImGuiKey_LeftCtrl, false);
        h.frame();
        h.chr('d');
        CHECK(!h.last);
        h.tap(ImGuiKey_Enter);
        CHECK(h.last && *h.last == "aB\nc\nd");

        // buffer cleared and focus kept after submit
        h.take();
        h.frame();
        h.chr('z');
        h.tap(ImGuiKey_Enter);
        CHECK(h.last && *h.last == "z");

        // whitespace-only is not submitted
        h.take();
        h.chr(' ');
        h.tap(ImGuiKey_Enter);
        CHECK(!h.last);

        // focus survives submit -> disabled frames -> enabled
        h.chr('q');
        h.tap(ImGuiKey_Enter);
        CHECK(h.last && *h.last == "q");
        h.frame();
        h.chr('p');
        h.key(ImGuiKey_Enter, true);
        h.frame(); // submit; the host disables input right after
        h.key(ImGuiKey_Enter, false);
        h.take();
        h.enabled = false;
        for(int i = 0; i < 3; ++i) h.frame();
        h.enabled = true;
        h.frame();
        h.frame();
        h.chr('y');
        h.tap(ImGuiKey_Enter);
        CHECK(h.last && *h.last == "y");
    }

    {
        // open state survives a collapsed host window (child is skipped)
        Harness h;
        auto& m = h.view.model();
        const size_t i = m.add({ "assistant", "x", {}, false });
        for(int k = 0; k < 5; ++k) m.add_item(i, { "t", "b", true, false });
        for(int k = 0; k < 3; ++k) h.frame();
        h.collapsed = true;
        for(int k = 0; k < 3; ++k) h.frame();
        for(int k = 0; k < 5; ++k) CHECK(m.item(i, k).open);
    }

    ImGui::DestroyContext();
    TEST_RETURN();
}
