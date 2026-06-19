// orbit camera: pivot-centred rotate / pan / zoom with auto-fit
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

struct Bounds;

enum class UpAxis { Y, Z };

class Camera {
public:
    Camera();

    [[nodiscard]] Magnum::Matrix4 view() const; // world -> camera
    [[nodiscard]] Magnum::Matrix4 projection(float aspect) const;
    [[nodiscard]] Magnum::Vector3 eye() const;

    void orbit(float dx, float dy); // screen-pixel deltas
    void pan(float dx, float dy);
    void zoom(float delta); // + = closer
    void fit(const Bounds& b, float margin = 1.5f);

    [[nodiscard]] const Magnum::Vector3& pivot() const { return _pivot; }
    [[nodiscard]] float distance() const { return _distance; }

    void set_fov_deg(float d) { _fov_deg = d; }
    void set_clip(float n, float f) {
        _near = n;
        _far = f;
    }
    void set_up_axis(UpAxis a) { _up = a; }

private:
    [[nodiscard]] Magnum::Vector3 up_vector() const;

    Magnum::Vector3 _pivot{ 0.0f };
    float _distance{ 5.0f };
    float _yaw{ 0.6f }; // radians, around up axis
    float _pitch{ 0.4f }; // radians, elevation
    float _fov_deg{ 45.0f };
    float _near{ 0.05f };
    float _far{ 500.0f };
    UpAxis _up{ UpAxis::Y };
};

} // namespace smg
