// orbit camera: pivot-centred rotate / pan / zoom with auto-fit
#pragma once

#include <Magnum/Magnum.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace smg {

struct Bounds;

enum class UpAxis { Y, Z };

class Camera {
public:
    enum class Projection { Perspective, Orthographic };

    Camera();

    [[nodiscard]] Magnum::Matrix4 view() const; // world -> camera
    [[nodiscard]] Magnum::Matrix4 projection(float aspect) const;
    [[nodiscard]] Magnum::Vector3 eye() const;

    void set_projection(Projection p) { _projection = p; }
    [[nodiscard]] Projection projection_mode() const { return _projection; }
    void iso(); // orthographic + 2:1 dimetric preset

    struct Ray {
        Magnum::Vector3 origin;
        Magnum::Vector3 direction;
    };

    // screen pixels (origin top-left, y-down) -> world-space ray; works for both projection modes
    [[nodiscard]] Ray unproject(const Magnum::Vector2& screen_px, const Magnum::Vector2& viewport_px) const;

    void orbit(float dx, float dy); // screen-pixel deltas
    void pan(float dx, float dy);
    void zoom(float delta); // + = closer
    void fit(const Bounds& b, float margin = 1.5f);

    [[nodiscard]] const Magnum::Vector3& pivot() const { return _pivot; }
    [[nodiscard]] float distance() const { return _distance; }

    void set_fov_deg(float d) { _fov_deg = d; }
    // manual near/far; disables auto clip
    void set_clip(float n, float f) {
        _near = n;
        _far = f;
        _auto_clip = false;
    }
    // auto clip derives near/far each frame from the scene bounds so fit/zoom/pan never clip it
    void set_auto_clip(bool on) { _auto_clip = on; }
    [[nodiscard]] bool auto_clip() const { return _auto_clip; }
    void set_scene_bounds(const Bounds& b);
    [[nodiscard]] Magnum::Vector2 clip_range() const; // effective {near, far}
    void set_up_axis(UpAxis a) { _up = a; }
    [[nodiscard]] UpAxis up_axis() const { return _up; }

private:
    [[nodiscard]] Magnum::Vector3 up_vector() const;

    Magnum::Vector3 _pivot{ 0.0f };
    float _distance{ 5.0f };
    float _yaw{ 0.6f }; // radians, around up axis
    float _pitch{ 0.4f }; // radians, elevation
    float _fov_deg{ 45.0f };
    float _near{ 0.05f };
    float _far{ 500.0f };
    bool _auto_clip{ true };
    Magnum::Vector3 _scene_center{ 0.0f };
    float _scene_radius{ 0.0f }; // 0 = unknown -> manual near/far
    UpAxis _up{ UpAxis::Y };
    Projection _projection{ Projection::Perspective };
};

} // namespace smg
