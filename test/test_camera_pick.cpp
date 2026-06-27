#include "Camera.hh"
#include "IsoGrid.hh"
#include "SceneTypes.hh"
#include "test_util.hh"

#include <cmath>
#include <optional>

using smg::Camera;

int main() {
    Camera cam;
    smg::Bounds box;
    box.expand(Magnum::Vector3{ -1.0f, -1.0f, -1.0f });
    box.expand(Magnum::Vector3{ 1.0f, 1.0f, 1.0f });
    cam.fit(box);
    cam.iso();

    const Magnum::Vector2 vp{ 800.0f, 600.0f };

    // the screen-centre ray should hit the ground near the pivot's ground projection
    const Camera::Ray center = cam.unproject(vp * 0.5f, vp);
    const std::optional<Magnum::Vector3> hit = smg::ray_ground(center, cam.up_axis());
    CHECK(hit.has_value());
    CHECK(smgtest::approx(hit->y(), 0.0f)); // on the ground plane
    // near the pivot column (horizontal XZ distance)
    const float dx = hit->x() - cam.pivot().x();
    const float dz = hit->z() - cam.pivot().z();
    CHECK(std::sqrt(dx * dx + dz * dz) < 1.0f);

    // the ray direction is unit length and points downward (iso camera looks down)
    CHECK(smgtest::approx(center.direction.length(), 1.0f, 1e-3f));
    CHECK(center.direction.y() < 0.0f);

    TEST_RETURN();
}
