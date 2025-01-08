
// Callbacks.hh

#pragma once

#include <memory>
#include <iostream>

#include "ImGuiFileDialog.h"
#include "ImGuiFileDialogConfig.h"

namespace smg {
typedef std::shared_ptr<class Callbacks> ShCallbacksPr;

class Callbacks {

	// properties
protected:
	// methods
public:
	// constructor
	Callbacks();

	// destructor
	~Callbacks();

	// factory
	static ShCallbacksPr create();

	static std::string window_filedialog(bool& flag);
	static std::string window_filedialog(bool& flag, std::string extensions);
	static std::string window_filedialog(bool& flag, std::string extensions, std::string path);

};
} // namespace smg
