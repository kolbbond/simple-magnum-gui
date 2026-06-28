#include "Camera.hh"

#include <algorithm>
#include <cmath>

#include <Magnum/Math/Angle.h>
#include <Magnum/Math/Constants.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector4.h>

#include "SceneTypes.hh"

namespace smg {

namespace {
constexpr float kOrbitRate = 0.01f; // radians per pixel
constexpr float kPanRate = 0.0015f; // pivot units per pixel, scaled by distance
constexpr float kZoomRate = 0.1f;
constexpr float kPitchLimit = 1.55f; // ~89 deg
} // namespace

Camera::Camera() = default;

Magnum::Vector3 Camera::up_vector() const { return _up == UpAxis::Y ? Magnum::Vector3::yAxis() : Magnum::Vector3::zAxis(); }

Magnum::Vector3 Camera::eye() const {
    const float cp = std::cos(_pitch);
    const float sp = std::sin(_pitch);
    // direction from pivot toward eye; elevation along the up axis
    const Magnum::Vector3 dir = _up == UpAxis::Y ? Magnum::Vector3{ cp * std::sin(_yaw), sp, cp * std::cos(_yaw) }
                                                 : Magnum::Vector3{ cp * std::sin(_yaw), cp * std::cos(_yaw), sp };
    return _pivot + dir * _distance;
}

Magnum::Matrix4 Camera::view() const { return Magnum::Matrix4::lookAt(eye(), _pivot, up_vector()).invertedRigid(); }

Magnum::Matrix4 Camera::projection(float aspect) const {
    if(_projection == Projection::Orthographic) {
        // match the perspective framing at pivot depth so fit()/zoom() stay meaningful
        const float halfFov = float(Magnum::Rad{ Magnum::Deg{ _fov_deg } } * 0.5f);
        const float height = 2.0f * _distance * std::tan(halfFov);
        return Magnum::Matrix4::orthographicProjection(Magnum::Vector2{ height * aspect, height }, _near, _far);
    }
    return Magnum::Matrix4::perspectiveProjection(Magnum::Deg{ _fov_deg }, aspect, _near, _far);
}

void Camera::iso() {
    _projection = Projection::Orthographic;
    _yaw = float(Magnum::Constants::piHalf()) * 0.5f; // 45 deg
    _pitch = std::atan(0.5f); // ~26.57 deg, 2:1 dimetric
}

Camera::Ray Camera::unproject(const Magnum::Vector2& screen_px, const Magnum::Vector2& viewport_px) const {
    const float aspect = viewport_px.y() > 0.0f ? viewport_px.x() / viewport_px.y() : 1.0f;
    // pixel -> NDC; flip Y because the screen origin is top-left
    const float ndcx = 2.0f * screen_px.x() / viewport_px.x() - 1.0f;
    const float ndcy = 1.0f - 2.0f * screen_px.y() / viewport_px.y();
    const Magnum::Matrix4 invVP = (projection(aspect) * view()).inverted();

    const Magnum::Vector4 n = invVP * Magnum::Vector4{ ndcx, ndcy, -1.0f, 1.0f };
    const Magnum::Vector4 f = invVP * Magnum::Vector4{ ndcx, ndcy, 1.0f, 1.0f };
    const Magnum::Vector3 np = n.xyz() / n.w();
    const Magnum::Vector3 fp = f.xyz() / f.w();
    return Ray{ np, (fp - np).normalized() };
}

void Camera::orbit(float dx, float dy) {
    _yaw += dx * kOrbitRate;
    _pitch = std::clamp(_pitch - dy * kOrbitRate, -kPitchLimit, kPitchLimit);
}

void Camera::pan(float dx, float dy) {
    const Magnum::Vector3 forward = (_pivot - eye()).normalized();
    const Magnum::Vector3 right = Magnum::Math::cross(forward, up_vector()).normalized();
    const Magnum::Vector3 trueUp = Magnum::Math::cross(right, forward);
    const float scale = kPanRate * _distance;
    _pivot += (-dx * right + dy * trueUp) * scale;
}

void Camera::zoom(float delta) {
    _distance *= std::exp(-delta * kZoomRate);
    _distance = std::max(_distance, 1e-3f);
}

void Camera::fit(const Bounds& b, float margin) {
    if(b.empty()) return;
    _pivot = b.center();
    const float radius = std::max(b.diagonal() * 0.5f, 1e-3f);
    const float halfFov = float(Magnum::Rad{ Magnum::Deg{ _fov_deg } } * 0.5f);
    _distance = (radius * margin) / std::tan(halfFov);
}

} // namespace smg
