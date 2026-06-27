// sprite value types: anchors, blend modes, sheet frame-UV math (headless)
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Vector2.h>

namespace smg {

enum class Anchor { Center, BottomCenter };
enum class SpriteBlend { AlphaMask, Alpha, Additive };

// regular grid of frames over a sheet; top-left origin, row-major
struct SpriteGrid {
    int cols{ 1 };
    int rows{ 1 };

    [[nodiscard]] int count() const { return cols * rows; }

    // normalized UV rect for a frame; out-of-range clamps to frame 0
    [[nodiscard]] Magnum::Range2D frame_uv(int index) const {
        const int n = count();
        const int i = (index < 0 || index >= n) ? 0 : index;
        const int col = i % cols;
        const int row = i / cols;
        const float u = 1.0f / float(cols);
        const float v = 1.0f / float(rows);
        return Magnum::Range2D{ { float(col) * u, float(row) * v }, { float(col + 1) * u, float(row + 1) * v } };
    }
};

// local-quad shift so the chosen anchor lands on the sprite's world position
[[nodiscard]] inline Magnum::Vector2 anchor_offset(Anchor a) {
    return a == Anchor::BottomCenter ? Magnum::Vector2{ 0.0f, 0.5f } : Magnum::Vector2{ 0.0f, 0.0f };
}

} // namespace smg
