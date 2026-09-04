#include "SceneTypes.hh"
#include "test_util.hh"

using smg::Bounds;

int main() {
    Bounds unit;
    unit.expand(Magnum::Vector3{ -0.5f, -0.5f, -0.5f });
    unit.expand(Magnum::Vector3{ 0.5f, 0.5f, 0.5f });

    // translation moves the box, keeps its size
    const Bounds moved = smg::transformed(unit, Magnum::Matrix4::translation({ 1.5f, 0.0f, 0.0f }));
    CHECK(smgtest::approx(moved.center().x(), 1.5f));
    CHECK(smgtest::approx(moved.size().x(), 1.0f));

    // scaling grows it about the origin
    const Bounds scaled = smg::transformed(unit, Magnum::Matrix4::scaling({ 4.0f, 2.0f, 1.0f }));
    CHECK(smgtest::approx(scaled.size().x(), 4.0f));
    CHECK(smgtest::approx(scaled.size().y(), 2.0f));

    // 45 deg about Z tilts the box, so the enclosing AABB grows by sqrt(2)
    const Bounds rotated = smg::transformed(unit, Magnum::Matrix4::rotationZ(Magnum::Deg{ 45.0f }));
    CHECK(smgtest::approx(rotated.size().x(), std::sqrt(2.0f)));
    CHECK(smgtest::approx(rotated.size().z(), 1.0f));

    // identity is a no-op; an empty box stays empty under any transform
    const Bounds same = smg::transformed(unit, Magnum::Matrix4{});
    CHECK(smgtest::approx(same.min.x(), unit.min.x()));
    CHECK(smgtest::approx(same.max.z(), unit.max.z()));
    CHECK(smg::transformed(Bounds{}, Magnum::Matrix4::translation({ 9.0f, 9.0f, 9.0f })).empty());

    TEST_RETURN();
}
