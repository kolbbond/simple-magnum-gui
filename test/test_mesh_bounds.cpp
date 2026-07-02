#include "Mesh.hh"
#include "test_util.hh"

#include <Magnum/Primitives/Cube.h>
#include <Magnum/Trade/MeshData.h>

int main() {
    // cubeSolid spans [-1, 1] on each axis; bounds derived from CPU mesh data
    const Magnum::Trade::MeshData cube = Magnum::Primitives::cubeSolid();
    const smg::Bounds b = smg::compute_bounds(cube);
    CHECK(smgtest::approx(b.size().x(), 2.0f));
    CHECK(smgtest::approx(b.size().y(), 2.0f));
    CHECK(smgtest::approx(b.size().z(), 2.0f));
    CHECK(smgtest::approx(b.center().x(), 0.0f));
    TEST_RETURN();
}
