#include "SpriteSheet.hh"

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>

namespace smg {

SpriteSheet::SpriteSheet(Magnum::GL::Texture2D&& tex, const SpriteGrid& grid) : _texture{ std::move(tex) }, _grid{ grid } {}

ShSpriteSheetPr SpriteSheet::from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows) {
    return std::make_shared<SpriteSheet>(std::move(tex), SpriteGrid{ cols, rows });
}

ShSpriteSheetPr SpriteSheet::from_pixels(Corrade::Containers::ArrayView<const char> pixels,
    const Magnum::Vector2i& size,
    Magnum::PixelFormat format,
    int cols,
    int rows) {
    Magnum::GL::Texture2D tex;
#ifdef SMG_PLATFORM_WASM
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA;
#else
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA8;
#endif
    tex.setMinificationFilter(Magnum::GL::SamplerFilter::Nearest) // crisp prerendered sprites
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Nearest)
        .setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
        .setStorage(1, tf, size)
        .setSubImage(0, {}, Magnum::ImageView2D{ format, size, pixels });
    return from_texture(std::move(tex), cols, rows);
}

} // namespace smg
