#include "Camera.hh"
#include "SceneTypes.hh"
#include "test_util.hh"

#include <Magnum/Math/Vector4.h>

using smg::Bounds;
using smg::Camera;

int main() {
    Camera cam;

    // fit a unit cube: pivot at centre, looking at it, centre projects to NDC origin
    Bounds box;
    box.expand(Magnum::Vector3{ -1.0f, -1.0f, -1.0f });
    box.expand(Magnum::Vector3{ 1.0f, 1.0f, 1.0f });
    cam.fit(box);
    CHECK(smgtest::approx(cam.pivot().x(), 0.0f));
    CHECK(cam.distance() > 1.0f);

    const Magnum::Vector4 clip = cam.projection(1.0f) * cam.view() * Magnum::Vector4{ cam.pivot(), 1.0f };
    const Magnum::Vector3 ndc = clip.xyz() / clip.w();
    CHECK(smgtest::approx(ndc.x(), 0.0f));
    CHECK(smgtest::approx(ndc.y(), 0.0f));

    // orbit preserves distance from pivot
    const float d0 = (cam.eye() - cam.pivot()).length();
    cam.orbit(30.0f, 12.0f);
    const float d1 = (cam.eye() - cam.pivot()).length();
    CHECK(smgtest::approx(d0, d1, 1e-3f));

    // zoom: positive = closer, negative = farther
    const float before = cam.distance();
    cam.zoom(1.0f);
    CHECK(cam.distance() < before);
    cam.zoom(-2.0f);
    CHECK(cam.distance() > before);

    // pan moves the pivot
    const Magnum::Vector3 p0 = cam.pivot();
    cam.pan(10.0f, 0.0f);
    CHECK((cam.pivot() - p0).length() > 0.0f);

    TEST_RETURN();
}
