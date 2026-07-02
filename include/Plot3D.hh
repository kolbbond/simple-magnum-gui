// ergonomic 3D plotting over ImPlot3D: scoped Plot3D (groups series).
// Draws into the current ImGui window; needs an active ImGui+ImPlot3D frame.
#pragma once

#ifdef SMG_WITH_IMPLOT3D

#    include <cstddef>
#    include <vector>

#    include <Magnum/Magnum.h>
#    include <Magnum/Math/Vector2.h>

#    include "implot3d.h"

namespace smg {

class Plot3D {
public:
    explicit Plot3D(const char* title, const Magnum::Vector2& size = Magnum::Vector2{ -1.0f, 0.0f })
        : _open{ ImPlot3D::BeginPlot(title, ImVec2{ size.x(), size.y() }) } {}
    ~Plot3D() {
        if(_open) ImPlot3D::EndPlot();
    }
    Plot3D(const Plot3D&) = delete;
    Plot3D& operator=(const Plot3D&) = delete;

    explicit operator bool() const { return _open; }

    template <class T>
    void line(const char* label, const std::vector<T>& x, const std::vector<T>& y, const std::vector<T>& z) {
        const int n = min3(x.size(), y.size(), z.size());
        if(_open && n > 0) ImPlot3D::PlotLine(label, x.data(), y.data(), z.data(), n);
    }
    template <class T>
    void scatter(const char* label, const std::vector<T>& x, const std::vector<T>& y, const std::vector<T>& z) {
        const int n = min3(x.size(), y.size(), z.size());
        if(_open && n > 0) ImPlot3D::PlotScatter(label, x.data(), y.data(), z.data(), n);
    }
    template <class T>
    void surface(const char* label, const std::vector<T>& x, const std::vector<T>& y, const std::vector<T>& z, int nx, int ny) {
        // PlotSurface indexes x, y AND z with nx*ny elements (not axis vectors) —
        // all three must be at least that long or it reads out of bounds
        const std::size_t need = std::size_t(nx) * std::size_t(ny);
        if(_open && nx > 0 && ny > 0 && x.size() >= need && y.size() >= need && z.size() >= need)
            ImPlot3D::PlotSurface(label, x.data(), y.data(), z.data(), nx, ny);
    }

private:
    static int min3(std::size_t a, std::size_t b, std::size_t c) {
        const std::size_t m = a < b ? a : b;
        return int(m < c ? m : c);
    }

    bool _open{ false };
};

} // namespace smg

#endif // SMG_WITH_IMPLOT3D
