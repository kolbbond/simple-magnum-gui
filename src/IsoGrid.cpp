#include "IsoGrid.hh"

#include <cmath>
#include <optional>

namespace smg {

Magnum::Vector3 IsoGrid::to_world(const Magnum::Vector2i& cell) const {
    const float a = float(cell.x()) * tile_size;
    const float b = float(cell.y()) * tile_size;
    // ground plane is the pair of axes perpendicular to the up axis
    return up == UpAxis::Y ? Magnum::Vector3{ a, 0.0f, b } : Magnum::Vector3{ a, b, 0.0f };
}

Magnum::Vector2i IsoGrid::to_cell(const Magnum::Vector3& world) const {
    const float a = world.x();
    const float b = up == UpAxis::Y ? world.z() : world.y();
    return Magnum::Vector2i{ int(std::lround(a / tile_size)), int(std::lround(b / tile_size)) };
}

std::optional<Magnum::Vector3> ray_ground(const Camera::Ray& ray, UpAxis up) {
    const float dir_n = up == UpAxis::Y ? ray.direction.y() : ray.direction.z();
    const float org_n = up == UpAxis::Y ? ray.origin.y() : ray.origin.z();
    if(std::fabs(dir_n) < 1e-6f) return {}; // parallel to the ground
    const float t = -org_n / dir_n;
    if(t < 0.0f) return {}; // behind the ray origin
    return ray.origin + ray.direction * t;
}

} // namespace smg
