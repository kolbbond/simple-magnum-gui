#include "Capture.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <vector>

#include <Corrade/Containers/Pointer.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Corrade/Utility/Debug.h>
#include <Magnum/ImageView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/AbstractImageConverter.h>

namespace smg {

namespace {

std::string extension(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    std::string ext = dot == std::string::npos ? std::string{} : path.substr(dot + 1);
    for(char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

// rows top-down, 4 bytes per pixel
std::vector<std::uint8_t> topDownRgba(const Magnum::ImageView2D& image) {
    const Magnum::Vector2i size = image.size();
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size.x()) * size.y() * 4);
    const Corrade::Containers::StridedArrayView2D<const Magnum::Color4ub> pixels = image.pixels<Magnum::Color4ub>();
    for(int y = 0; y < size.y(); ++y)
        for(int x = 0; x < size.x(); ++x) {
            const Magnum::Color4ub c = pixels[size.y() - 1 - y][x];
            std::uint8_t* p = &out[(static_cast<std::size_t>(y) * size.x() + x) * 4];
            p[0] = c.r();
            p[1] = c.g();
            p[2] = c.b();
            p[3] = c.a();
        }
    return out;
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t n, std::uint32_t crc = 0) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for(std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for(int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for(std::size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void putBE32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v >> 24));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v));
}

void chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
    putBE32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    putBE32(out, crc32(&out[start], out.size() - start));
}

// PNG with deflate "stored" blocks: no compression, so no zlib needed
bool writePng(const Magnum::ImageView2D& image, const std::string& path) {
    const Magnum::Vector2i size = image.size();
    const std::vector<std::uint8_t> rgba = topDownRgba(image);
    const std::size_t row = static_cast<std::size_t>(size.x()) * 4;

    std::vector<std::uint8_t> raw; // filter byte 0 + row, per scanline
    raw.reserve((row + 1) * size.y());
    for(int y = 0; y < size.y(); ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + y * row, rgba.begin() + (y + 1) * row);
    }

    std::vector<std::uint8_t> z = { 0x78, 0x01 };
    std::uint32_t a = 1, b = 0; // adler32
    for(std::uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    for(std::size_t off = 0; off < raw.size() || off == 0;) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - off);
        const bool last = off + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n));
        z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n));
        z.push_back(static_cast<std::uint8_t>(~n >> 8));
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
        off += n;
        if(last) break;
    }
    putBE32(z, (b << 16) | a);

    std::vector<std::uint8_t> ihdr;
    putBE32(ihdr, static_cast<std::uint32_t>(size.x()));
    putBE32(ihdr, static_cast<std::uint32_t>(size.y()));
    ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 }); // 8-bit RGBA, deflate, adaptive filter, no interlace

    std::vector<std::uint8_t> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return bool(f);
}

// uncompressed 32-bit TGA; bottom-up rows are TGA's default origin, so no flip
bool writeTga(const Magnum::ImageView2D& image, const std::string& path) {
    const Magnum::Vector2i size = image.size();
    std::uint8_t header[18] = {};
    header[2] = 2;
    header[12] = static_cast<std::uint8_t>(size.x());
    header[13] = static_cast<std::uint8_t>(size.x() >> 8);
    header[14] = static_cast<std::uint8_t>(size.y());
    header[15] = static_cast<std::uint8_t>(size.y() >> 8);
    header[16] = 32;
    header[17] = 8;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(header), sizeof(header));
    const Corrade::Containers::StridedArrayView2D<const Magnum::Color4ub> pixels = image.pixels<Magnum::Color4ub>();
    for(int y = 0; y < size.y(); ++y)
        for(int x = 0; x < size.x(); ++x) {
            const Magnum::Color4ub c = pixels[y][x];
            const std::uint8_t bgra[4] = { c.b(), c.g(), c.r(), c.a() };
            f.write(reinterpret_cast<const char*>(bgra), 4);
        }
    return bool(f);
}

} // namespace

bool save_image(const Magnum::ImageView2D& image, const std::string& path) {
    if(image.format() != Magnum::PixelFormat::RGBA8Unorm) {
        Corrade::Utility::Error{} << "smg: save_image expects RGBA8Unorm, got" << image.format();
        return false;
    }

    // plugins are optional; keep their "not found" chatter out of the log
    {
        Corrade::Utility::Warning silenceWarnings{ nullptr };
        Corrade::Utility::Error silenceErrors{ nullptr };
        Corrade::PluginManager::Manager<Magnum::Trade::AbstractImageConverter> manager;
        Corrade::Containers::Pointer<Magnum::Trade::AbstractImageConverter> converter = manager.loadAndInstantiate("AnyImageConverter");
        if(converter && converter->convertToFile(image, path)) return true;
    }

    const std::string ext = extension(path);
    if(ext == "png") return writePng(image, path);
    if(ext == "tga") return writeTga(image, path);
    Corrade::Utility::Error{} << "smg: cannot save" << path.c_str() << "- without image converter plugins only .png and .tga work";
    return false;
}

} // namespace smg
