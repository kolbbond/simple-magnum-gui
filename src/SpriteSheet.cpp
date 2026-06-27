#include "SpriteSheet.hh"

#include <Magnum/GL/TextureFormat.h>
#include <Magnum/ImageView.h>

#ifdef SMG_WITH_IMAGE_IMPORT
#    include <Corrade/Containers/Optional.h>
#    include <Corrade/Containers/Pointer.h>
#    include <Corrade/PluginManager/Manager.h>
#    include <Magnum/Trade/AbstractImporter.h>
#    include <Magnum/Trade/ImageData.h>
#endif

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

#ifdef SMG_WITH_IMAGE_IMPORT
ShSpriteSheetPr SpriteSheet::load(const char* path, int cols, int rows) {
    Corrade::PluginManager::Manager<Magnum::Trade::AbstractImporter> manager{ SMG_IMPORTER_DIR };
    Corrade::Containers::Pointer<Magnum::Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
    if(!importer || !importer->openFile(path)) return nullptr;
    const Corrade::Containers::Optional<Magnum::Trade::ImageData2D> image = importer->image2D(0);
    if(!image) return nullptr;
    return from_pixels(image->data(), image->size(), image->format(), cols, rows);
}
#endif

} // namespace smg
