#include "SceneTypes.hh"
#include "test_util.hh"

#include <vector>

using smg::Bounds;
using smg::Vertex;

int main() {
    // expand grows the box; center/size/diagonal are correct
    Bounds b;
    CHECK(b.empty());
    b.expand(Magnum::Vector3{ -1.0f, -2.0f, -3.0f });
    b.expand(Magnum::Vector3{ 1.0f, 2.0f, 3.0f });
    CHECK(!b.empty());
    CHECK(smgtest::approx(b.center().x(), 0.0f));
    CHECK(smgtest::approx(b.center().y(), 0.0f));
    CHECK(smgtest::approx(b.size().x(), 2.0f));
    CHECK(smgtest::approx(b.size().z(), 6.0f));
    CHECK(smgtest::approx(b.diagonal(), std::sqrt(4.0f + 16.0f + 36.0f)));

    // compute_bounds over a vertex list
    std::vector<Vertex> verts{ Vertex{ Magnum::Vector3{ 0.0f, 0.0f, 0.0f }, {}, {} },
        Vertex{ Magnum::Vector3{ 4.0f, 0.0f, 0.0f }, {}, {} },
        Vertex{ Magnum::Vector3{ 0.0f, 5.0f, 0.0f }, {}, {} } };
    Bounds cb = smg::compute_bounds({ verts.data(), verts.size() });
    CHECK(smgtest::approx(cb.size().x(), 4.0f));
    CHECK(smgtest::approx(cb.size().y(), 5.0f));

    TEST_RETURN();
}
