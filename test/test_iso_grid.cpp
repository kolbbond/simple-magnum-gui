#include "IsoGrid.hh"
#include "test_util.hh"

int main() {
    // Y-up: cells map onto the XZ ground plane
    const smg::IsoGrid g{ 2.0f, smg::UpAxis::Y };
    const Magnum::Vector3 w = g.to_world(Magnum::Vector2i{ 3, -1 });
    CHECK(smgtest::approx(w.x(), 6.0f));
    CHECK(smgtest::approx(w.y(), 0.0f));
    CHECK(smgtest::approx(w.z(), -2.0f));

    // round-trip through the nearest cell
    const Magnum::Vector2i c = g.to_cell(Magnum::Vector3{ 5.9f, 0.0f, -2.1f });
    CHECK(c.x() == 3);
    CHECK(c.y() == -1);

    // Z-up: cells map onto the XY ground plane
    const smg::IsoGrid gz{ 1.0f, smg::UpAxis::Z };
    const Magnum::Vector3 wz = gz.to_world(Magnum::Vector2i{ 2, 4 });
    CHECK(smgtest::approx(wz.x(), 2.0f));
    CHECK(smgtest::approx(wz.y(), 4.0f));
    CHECK(smgtest::approx(wz.z(), 0.0f));

    // degenerate tile_size must not divide by zero in to_cell (regression)
    const smg::IsoGrid gd{ 0.0f, smg::UpAxis::Y };
    const Magnum::Vector2i cd = gd.to_cell(Magnum::Vector3{ 3.0f, 0.0f, 5.0f });
    CHECK(cd.x() == 3); // falls back to unit tile
    CHECK(cd.y() == 5);

    TEST_RETURN();
}
