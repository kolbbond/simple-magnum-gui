#include "FileDialog.hh"

#include <memory>
#include <utility>

#include "ImGuiFileDialog.h"

namespace smg {

FileDialog::FileDialog(std::string key) : _dialog(std::make_unique<IGFD::FileDialog>()), _key(std::move(key)) {}

FileDialog::~FileDialog() = default;

void FileDialog::open(const std::string& title, const char* filters, const std::string& start_path) {
    _valid = false; // a fresh launch invalidates the previous result
    IGFD::FileDialogConfig config;
    config.path = start_path;
    config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_DontShowHiddenFiles;
    _dialog->OpenDialog(_key, title, filters, config);
}

void FileDialog::save(const std::string& title, const char* filters, const std::string& start_path) {
    _valid = false;
    IGFD::FileDialogConfig config;
    config.path = start_path;
    config.fileName = "untitled";
    config.flags = ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite;
    _dialog->OpenDialog(_key, title, filters, config);
}

bool FileDialog::draw() {
    bool chosen = false;
    if(_dialog->Display(_key)) {
        if(_dialog->IsOk()) {
            _path = _dialog->GetFilePathName();
            _valid = true;
            chosen = true;
        }
        _dialog->Close();
    }
    return chosen;
}

} // namespace smg
