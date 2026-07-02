#include "SceneTypes.hh"

#include <limits>

namespace smg {

Bounds::Bounds() : min{ std::numeric_limits<float>::max() }, max{ std::numeric_limits<float>::lowest() } {}

bool Bounds::empty() const { return min.x() > max.x(); }

void Bounds::expand(const Magnum::Vector3& p) {
    min = Magnum::Math::min(min, p);
    max = Magnum::Math::max(max, p);
}

void Bounds::expand(const Bounds& b) {
    if(b.empty()) return;
    expand(b.min);
    expand(b.max);
}

Magnum::Vector3 Bounds::center() const { return (min + max) * 0.5f; }

Magnum::Vector3 Bounds::size() const { return max - min; }

float Bounds::diagonal() const { return size().length(); }

Bounds compute_bounds(Corrade::Containers::ArrayView<const Vertex> verts) {
    Bounds b;
    for(const Vertex& v : verts) b.expand(v.position);
    return b;
}

} // namespace smg
