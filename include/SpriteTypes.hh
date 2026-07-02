// sprite value types: anchors, blend modes, sheet frame-UV math (headless)
#pragma once

#include <cmath>
#include <memory>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Range.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

enum class Anchor { Center, BottomCenter };
enum class SpriteBlend { AlphaMask, Alpha, Additive };

// regular grid of frames over a sheet; top-left origin, row-major
struct SpriteGrid {
    int cols{ 1 };
    int rows{ 1 };

    // clamped to >= 1 so a degenerate grid can't divide by zero
    [[nodiscard]] int cols_safe() const { return cols < 1 ? 1 : cols; }
    [[nodiscard]] int rows_safe() const { return rows < 1 ? 1 : rows; }
    [[nodiscard]] int count() const { return cols_safe() * rows_safe(); }

    // normalized UV rect for a frame; out-of-range clamps to frame 0
    [[nodiscard]] Magnum::Range2D frame_uv(int index) const {
        const int c = cols_safe();
        const int r = rows_safe();
        const int n = c * r;
        const int i = (index < 0 || index >= n) ? 0 : index;
        const int col = i % c;
        const int row = i / c;
        const float u = 1.0f / float(c);
        const float v = 1.0f / float(r);
        return Magnum::Range2D{ { float(col) * u, float(row) * v }, { float(col + 1) * u, float(row + 1) * v } };
    }
};

// local-quad shift so the chosen anchor lands on the sprite's world position
[[nodiscard]] inline Magnum::Vector2 anchor_offset(Anchor a) {
    return a == Anchor::BottomCenter ? Magnum::Vector2{ 0.0f, 0.5f } : Magnum::Vector2{ 0.0f, 0.0f };
}

// frame-range animation clip; advances by wall-clock seconds, loops
struct SpriteClip {
    int first{ 0 };
    int last{ 0 };
    float fps{ 0.0f };

    [[nodiscard]] int frame_at(float t) const {
        const int span = last - first + 1;
        if(span <= 1 || fps <= 0.0f) return first;
        // wrap in the float domain first: t*fps can exceed INT_MAX for long-running
        // clocks, and float->int is UB past 2^31. fmod keeps the value in [0, span).
        float phase = std::fmod(std::floor(t * fps), float(span));
        if(phase < 0.0f) phase += float(span);
        return first + int(phase);
    }
};

// facing angle (degrees, CCW) -> sheet row in [0, num_dirs); wraps
[[nodiscard]] inline int dir_row(float facing_deg, int num_dirs) {
    if(num_dirs <= 0) return 0;
    const float step = 360.0f / float(num_dirs);
    const int r = int(std::lround(facing_deg / step));
    return ((r % num_dirs) + num_dirs) % num_dirs;
}

class SpriteSheet;
typedef std::shared_ptr<SpriteSheet> ShSpriteSheetPr;

struct SpriteParams {
    Magnum::Vector2 size{ 1.0f, 1.0f }; // world units (W, H)
    Anchor anchor{ Anchor::BottomCenter };
    Magnum::Color4 tint{ 1.0f };
    SpriteBlend blend{ SpriteBlend::AlphaMask };
    bool visible{ true };
};

struct Sprite {
    ShSpriteSheetPr sheet;
    int frame{ 0 };
    Magnum::Vector3 position{ 0.0f };
    SpriteParams params;
};

} // namespace smg
