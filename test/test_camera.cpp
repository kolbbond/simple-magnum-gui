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

    // orthographic mode: the pivot still projects to the NDC origin
    cam.set_projection(Camera::Projection::Orthographic);
    CHECK(cam.projection_mode() == Camera::Projection::Orthographic);
    const Magnum::Vector4 oclip = cam.projection(1.0f) * cam.view() * Magnum::Vector4{ cam.pivot(), 1.0f };
    const Magnum::Vector3 ondc = oclip.xyz() / oclip.w();
    CHECK(smgtest::approx(ondc.x(), 0.0f));
    CHECK(smgtest::approx(ondc.y(), 0.0f));

    // ortho projection has no perspective foreshortening: w == 1
    CHECK(smgtest::approx(oclip.w(), 1.0f));

    // iso preset selects orthographic and the 2:1 dimetric pitch
    Camera iso;
    iso.iso();
    CHECK(iso.projection_mode() == Camera::Projection::Orthographic);
    // sin(atan(0.5)) ~= 0.447 -> eye elevation along the up axis for 2:1 dimetric
    const Magnum::Vector3 d = (iso.eye() - iso.pivot()).normalized();
    CHECK(smgtest::approx(d.y(), 0.4472f, 1e-2f));

    TEST_RETURN();
}
