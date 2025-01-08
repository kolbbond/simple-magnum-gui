#include "Callbacks.hh"

namespace smg {
std::string Callbacks::window_filedialog(bool& flag, std::string extensions, std::string path) {
	IGFD::FileDialogConfig config;
	config.path = path;
	ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", extensions.c_str(), config);
	std::string fpathname;
	std::string fpath;
	// display
	if(ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
		if(ImGuiFileDialog::Instance()->IsOk()) {
			fpathname = ImGuiFileDialog::Instance()->GetFilePathName();
			fpath = ImGuiFileDialog::Instance()->GetCurrentPath();
			std::cout << "file path: " << fpathname << std::endl;
			std::cout << "current path: " << fpath << std::endl;
		}

		// close
		ImGuiFileDialog::Instance()->Close();
		flag = false;
	}

	return fpathname;
}
std::string Callbacks::window_filedialog(bool& flag, std::string extensions) {
	// wrapper
	return Callbacks::window_filedialog(flag, extensions, ".");
}
std::string Callbacks::window_filedialog(bool& flag) {
	return Callbacks::window_filedialog(flag, ".wav", ".");
}
} // namespace smg
