// thin wrapper over ImGuiFileDialog: open/save pickers without the singleton
// ceremony. Construct one, call open()/save() to launch, draw() every ImGui
// frame, and read path() once draw() returns true.
#pragma once

#include <memory>
#include <string>

namespace IGFD {
class FileDialog;
}

namespace smg {

class FileDialog {
public:
    explicit FileDialog(std::string key = "smgFileDialog");
    ~FileDialog(); // defined in .cpp where IGFD::FileDialog is complete (unique_ptr)

    // owns a private dialog (stable identity, holds a GL/plugin resource) -> not copyable/movable
    FileDialog(const FileDialog&) = delete;
    FileDialog& operator=(const FileDialog&) = delete;

    // launch an open-file picker (choose an existing file)
    void open(const std::string& title, const char* filters = ".*", const std::string& start_path = ".");
    // launch a save-file picker (type a new name; confirms before overwrite)
    void save(const std::string& title, const char* filters = ".*", const std::string& start_path = ".");

    // call once per ImGui frame while a dialog may be open;
    // returns true the frame a path is confirmed (Ok pressed)
    bool draw();

    [[nodiscard]] const std::string& path() const { return _path; }
    [[nodiscard]] bool valid() const { return _valid; }

private:
    // a private instance instead of the shared ImGuiFileDialog singleton, so two
    // smg::FileDialog objects can't clobber each other's state or steal each
    // other's result when they share a key
    std::unique_ptr<IGFD::FileDialog> _dialog;
    std::string _key;
    std::string _path;
    bool _valid{ false };
};

} // namespace smg
