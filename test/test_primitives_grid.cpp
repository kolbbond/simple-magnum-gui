#include "Primitives.hh"
#include "test_util.hh"

int main() {
    const Magnum::Color3 col{ 0.25f, 0.5f, 0.75f };
    const std::vector<smg::Vertex> v = smg::primitives::grid_vertices({ 2, 2 }, col);

    // non-empty and even (line endpoint pairs)
    CHECK(!v.empty());
    CHECK(v.size() % 2 == 0);

    // every vertex lies on the XZ plane (y == 0)
    for(const smg::Vertex& vert : v) { CHECK(smgtest::approx(vert.position.y(), 0.0f)); }

    // every vertex carries the requested color
    for(const smg::Vertex& vert : v) {
        CHECK(smgtest::approx(vert.color.r(), col.r()));
        CHECK(smgtest::approx(vert.color.g(), col.g()));
        CHECK(smgtest::approx(vert.color.b(), col.b()));
    }

    // geometry stays within [-1, 1] on X and Z
    for(const smg::Vertex& vert : v) {
        CHECK(vert.position.x() >= -1.0f - 1e-4f && vert.position.x() <= 1.0f + 1e-4f);
        CHECK(vert.position.z() >= -1.0f - 1e-4f && vert.position.z() <= 1.0f + 1e-4f);
    }

    TEST_RETURN();
}
