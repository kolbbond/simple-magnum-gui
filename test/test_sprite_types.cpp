#include "SpriteTypes.hh"
#include "test_util.hh"

int main() {
    const smg::SpriteGrid g{ 4, 2 }; // 4 cols, 2 rows => 8 frames
    CHECK(g.count() == 8);

    // frame 0 is the top-left cell
    const Magnum::Range2D f0 = g.frame_uv(0);
    CHECK(smgtest::approx(f0.min().x(), 0.0f));
    CHECK(smgtest::approx(f0.min().y(), 0.0f));
    CHECK(smgtest::approx(f0.max().x(), 0.25f));
    CHECK(smgtest::approx(f0.max().y(), 0.5f));

    // frame 5 = col 1, row 1 (row-major, top-left origin)
    const Magnum::Range2D f5 = g.frame_uv(5);
    CHECK(smgtest::approx(f5.min().x(), 0.25f));
    CHECK(smgtest::approx(f5.min().y(), 0.5f));
    CHECK(smgtest::approx(f5.max().x(), 0.5f));
    CHECK(smgtest::approx(f5.max().y(), 1.0f));

    // out-of-range clamps to frame 0 (defensive, no UB)
    const Magnum::Range2D fbad = g.frame_uv(999);
    CHECK(smgtest::approx(fbad.min().x(), 0.0f));

    // anchor offsets
    CHECK(smgtest::approx(smg::anchor_offset(smg::Anchor::Center).y(), 0.0f));
    CHECK(smgtest::approx(smg::anchor_offset(smg::Anchor::BottomCenter).y(), 0.5f));

    // degenerate grid must not divide by zero (regression: 0 % 0 SIGFPE)
    const smg::SpriteGrid gz{ 0, 0 };
    CHECK(gz.count() == 1); // clamped to a 1x1 grid
    const Magnum::Range2D fz = gz.frame_uv(3);
    CHECK(smgtest::approx(fz.min().x(), 0.0f));
    CHECK(smgtest::approx(fz.max().x(), 1.0f));

    // large elapsed time must not overflow float->int in frame_at
    const smg::SpriteClip clip{ 0, 3, 24.0f }; // 4 frames
    CHECK(clip.frame_at(0.0f) == 0);
    CHECK(clip.frame_at(1.0f / 24.0f) == 1);
    const int big = clip.frame_at(1.0e9f); // ~31 years at 24fps; must stay in [0,3]
    CHECK(big >= 0 && big <= 3);

    TEST_RETURN();
}
