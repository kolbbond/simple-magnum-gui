// file_dialog example: buttons open/save via smg::FileDialog; show the path.
#include "FileDialog.hh"
#include "GuiBase.hh"

#include <imgui.h>

#include <string>

using namespace Magnum;
using namespace smg;

struct State {
    FileDialog dialog;
    std::string picked;
};

int dialog_cb(State& state) {
    ImGui::Begin("File Dialog");
    if(ImGui::Button("Open File...")) state.dialog.open("Choose a file", ".cpp,.hh,.*");
    ImGui::SameLine();
    if(ImGui::Button("Save File...")) state.dialog.save("Save as", ".*");
    if(state.dialog.draw()) state.picked = state.dialog.path();
    if(!state.picked.empty()) ImGui::Text("Picked: %s", state.picked.c_str());
    ImGui::End();
    return 0;
}

class FileDialogExample: public GuiBase {
public:
    explicit FileDialogExample(const Arguments& arguments) : GuiBase(arguments) {
        add_callback([this]() { return dialog_cb(_state); });
    }

private:
    State _state;
};

MAGNUM_APPLICATION_MAIN(FileDialogExample)
