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

int dialog_cb(void* data) {
    State* s = reinterpret_cast<State*>(data);
    ImGui::Begin("File Dialog");
    if(ImGui::Button("Open File...")) s->dialog.open("Choose a file", ".cpp,.hh,.*");
    ImGui::SameLine();
    if(ImGui::Button("Save File...")) s->dialog.save("Save as", ".*");
    if(s->dialog.draw()) s->picked = s->dialog.path();
    if(!s->picked.empty()) ImGui::Text("Picked: %s", s->picked.c_str());
    ImGui::End();
    return 0;
}

class FileDialogExample: public GuiBase {
public:
    explicit FileDialogExample(const Arguments& arguments) : GuiBase(arguments) {
        _cb = DrawCallback::create();
        _cb->set_callback(dialog_cb);
        _cb->set_data(&_state);
        add_callback(_cb);
    }

private:
    State _state;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(FileDialogExample)
