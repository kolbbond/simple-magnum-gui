// gui test: ChatView over 200 mixed messages, streaming a delta every frame
#include "ChatView.hh"
#include "GuiBase.hh"
#include "gui_test_util.hh"
#include "imgui.h"
#include "test_util.hh"

#include <cstdlib>
#include <string>

using namespace smg;

int main(int argc, char** argv) {
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);

    ChatView view;
    view.set_role_color("user", ImVec4(0.5f, 0.8f, 1.f, 1.f));
    ChatModel& m = view.model();
    for(int i = 0; i < 200; ++i) {
        if(i % 50 == 25) {
            m.add({ "", "switched agent", {}, true });
            continue;
        }
        const size_t idx = m.add(
            { i % 2 ? "assistant" : "user", "message " + std::to_string(i) + " with some long wrapping text to exercise it", {}, false });
        if(i % 3 == 0) m.append_text(idx, "\n```\ncode line 1\n  code line 2\n```\nafter");
        if(i % 4 == 0) m.add_item(idx, { "tool call", "body", i % 8 == 0, false });
        if(i % 7 == 0) m.add_item(idx, { "failed", "boom", false, true });
    }
    const size_t live = m.add({ "assistant", "", {}, false });
    size_t submitted = 0;

    gui.add_callback([&]() {
        m.append_text(live, "tok ");
        ImGui::Begin("chat");
        if(view.draw(true)) ++submitted;
        ImGui::End();
        ImGui::Begin("chat disabled");
        view.draw(false);
        ImGui::End();
        return 0;
    });

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();
    gui.exit();
    CHECK(submitted == 0);
    CHECK(m.messages().size() == 201);
    std::exit(smgtest::failures() ? 1 : 0);
}
