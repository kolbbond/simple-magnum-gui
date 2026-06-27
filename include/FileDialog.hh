// thin wrapper over ImGuiFileDialog: open/save pickers without the singleton
// ceremony. Construct one, call open()/save() to launch, draw() every ImGui
// frame, and read path() once draw() returns true.
#pragma once

#include <string>

namespace smg {

class FileDialog {
public:
    explicit FileDialog(std::string key = "smgFileDialog");

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
    std::string _key;
    std::string _path;
    bool _valid{ false };
};

} // namespace smg
