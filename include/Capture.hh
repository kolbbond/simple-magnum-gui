// saving framebuffer images
#pragma once

#include <string>

#include <Magnum/Magnum.h>

namespace smg {

// writes an RGBA8 image (Magnum's Y-up convention) to `path`. Magnum's AnyImageConverter is tried
// first (compressed PNG, JPEG, ... when the plugins are installed); without it .png and .tga are
// written by a built-in encoder (uncompressed). false on an unsupported extension or I/O error
bool save_image(const Magnum::ImageView2D& image, const std::string& path);

} // namespace smg
