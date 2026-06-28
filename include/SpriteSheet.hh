// sprite sheet: a GL texture plus a frame grid over it
#pragma once

#include <memory>

#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/PixelFormat.h>

#include "SpriteTypes.hh"

namespace smg {

class SpriteSheet {
public:
    SpriteSheet(Magnum::GL::Texture2D&& tex, const SpriteGrid& grid);

    static ShSpriteSheetPr from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows);
    static ShSpriteSheetPr from_pixels(Corrade::Containers::ArrayView<const char> pixels,
        const Magnum::Vector2i& size,
        Magnum::PixelFormat format,
        int cols,
        int rows);

#ifdef SMG_WITH_IMAGE_IMPORT
    static ShSpriteSheetPr load(const char* path, int cols, int rows);
#endif

    [[nodiscard]] const SpriteGrid& grid() const { return _grid; }
    [[nodiscard]] Magnum::GL::Texture2D& texture() { return _texture; }
    [[nodiscard]] Magnum::Range2D frame_uv(int index) const { return _grid.frame_uv(index); }

private:
    Magnum::GL::Texture2D _texture;
    SpriteGrid _grid;
};

} // namespace smg
