#include "SpriteSheet.hh"

#include <cstddef>

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelFormat.h>

#ifdef SMG_WITH_IMAGE_IMPORT
#    include <Corrade/Containers/Optional.h>
#    include <Corrade/Containers/Pointer.h>
#    include <Corrade/PluginManager/Manager.h>
#    include <Magnum/Trade/AbstractImporter.h>
#    include <Magnum/Trade/ImageData.h>
#endif

namespace smg {

namespace {
// view carries its own PixelStorage, so non-default row alignment uploads correctly
Magnum::GL::Texture2D make_texture(const Magnum::ImageView2D& view) {
#if defined(MAGNUM_TARGET_GLES2)
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA; // WebGL1/GLES2: texStorage needs unsized
#else
    const Magnum::GL::TextureFormat tf = Magnum::GL::TextureFormat::RGBA8; // desktop + WebGL2: sized
#endif
    Magnum::GL::Texture2D tex;
    tex.setMinificationFilter(Magnum::GL::SamplerFilter::Nearest) // crisp prerendered sprites
        .setMagnificationFilter(Magnum::GL::SamplerFilter::Nearest)
        .setWrapping(Magnum::GL::SamplerWrapping::ClampToEdge)
        .setStorage(1, tf, view.size())
        .setSubImage(0, {}, view);
    return tex;
}
} // namespace

SpriteSheet::SpriteSheet(Magnum::GL::Texture2D&& tex, const SpriteGrid& grid) : _texture{ std::move(tex) }, _grid{ grid } {}

ShSpriteSheetPr SpriteSheet::from_texture(Magnum::GL::Texture2D&& tex, int cols, int rows) {
    return std::make_shared<SpriteSheet>(std::move(tex), SpriteGrid{ cols, rows });
}

ShSpriteSheetPr SpriteSheet::from_pixels(Corrade::Containers::ArrayView<const char> pixels,
    const Magnum::Vector2i& size,
    Magnum::PixelFormat format,
    int cols,
    int rows) {
    // reject degenerate/truncated input; Magnum's size assert is compiled out in release
    if(size.x() <= 0 || size.y() <= 0) return nullptr;
    const std::size_t need = std::size_t(size.x()) * std::size_t(size.y()) * Magnum::pixelFormatSize(format);
    if(pixels.size() < need) return nullptr;
    return from_texture(make_texture(Magnum::ImageView2D{ format, size, pixels }), cols, rows);
}

#ifdef SMG_WITH_IMAGE_IMPORT
ShSpriteSheetPr SpriteSheet::load(const char* path, int cols, int rows) {
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager{ SMG_IMPORTER_DIR };
    Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
    if(!importer || !importer->openFile(path)) return nullptr;
    const Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image = importer->image2D(0);
    if(!image) return nullptr;
    // upload directly so the image's PixelStorage is honored (rebuilding a view with
    // default alignment sheared odd-width RGB rows)
    return from_texture(make_texture(*image), cols, rows);
}
#endif

} // namespace smg
