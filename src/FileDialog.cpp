#include "FileDialog.hh"

#include <utility>

#include "ImGuiFileDialog.h"

namespace smg {

FileDialog::FileDialog(std::string key) : _key(std::move(key)) {}

void FileDialog::open(const std::string& title, const char* filters, const std::string& start_path) {
    IGFD::FileDialogConfig config;
    config.path = start_path;
    config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_DontShowHiddenFiles;
    ImGuiFileDialog::Instance()->OpenDialog(_key, title, filters, config);
}

void FileDialog::save(const std::string& title, const char* filters, const std::string& start_path) {
    IGFD::FileDialogConfig config;
    config.path = start_path;
    config.fileName = "untitled";
    config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog(_key, title, filters, config);
}

bool FileDialog::draw() {
    bool chosen = false;
    if(ImGuiFileDialog::Instance()->Display(_key)) {
        if(ImGuiFileDialog::Instance()->IsOk()) {
            _path = ImGuiFileDialog::Instance()->GetFilePathName();
            _valid = true;
            chosen = true;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    return chosen;
}

} // namespace smg
