#include "Primitives.hh"
#include "test_util.hh"

int main() {
    // three axis lines: 6 vertices, each pair coloured R/G/B, tips at `scale`
    const std::vector<smg::Vertex> v = smg::primitives::axes_vertices(2.0f);
    CHECK(v.size() == 6);

    // +X line red, tip at (2,0,0)
    CHECK(smgtest::approx(v[0].color.r(), 1.0f));
    CHECK(smgtest::approx(v[0].color.g(), 0.0f));
    CHECK(smgtest::approx(v[1].position.x(), 2.0f));

    // +Y line green, tip at (0,2,0)
    CHECK(smgtest::approx(v[2].color.g(), 1.0f));
    CHECK(smgtest::approx(v[3].position.y(), 2.0f));

    // +Z line blue, tip at (0,0,2)
    CHECK(smgtest::approx(v[4].color.b(), 1.0f));
    CHECK(smgtest::approx(v[5].position.z(), 2.0f));

    TEST_RETURN();
}
