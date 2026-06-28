// ergonomic 2D plotting over ImPlot: scoped Plot (groups series) + one-liners.
// Draws into the current ImGui window; needs an active ImGui+ImPlot frame.
#pragma once

#include <cstddef>
#include <vector>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

#include "implot.h"

namespace smg {

class Plot {
public:
    explicit Plot(const char* title, const Magnum::Vector2& size = Magnum::Vector2{ -1.0f, 0.0f })
        : _open{ ImPlot::BeginPlot(title, ImVec2{ size.x(), size.y() }) } {}
    ~Plot() {
        if(_open) ImPlot::EndPlot();
    }
    Plot(const Plot&) = delete;
    Plot& operator=(const Plot&) = delete;

    explicit operator bool() const { return _open; }

    template <class T>
    void line(const char* label, const std::vector<T>& y) {
        if(_open && !y.empty()) ImPlot::PlotLine(label, y.data(), int(y.size()));
    }
    template <class T>
    void line(const char* label, const std::vector<T>& x, const std::vector<T>& y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotLine(label, x.data(), y.data(), n);
    }
    template <class T>
    void scatter(const char* label, const std::vector<T>& y) {
        if(_open && !y.empty()) ImPlot::PlotScatter(label, y.data(), int(y.size()));
    }
    template <class T>
    void scatter(const char* label, const std::vector<T>& x, const std::vector<T>& y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotScatter(label, x.data(), y.data(), n);
    }
    template <class T>
    void bar(const char* label, const std::vector<T>& y, double width = 0.67) {
        if(_open && !y.empty()) ImPlot::PlotBars(label, y.data(), int(y.size()), width);
    }
    template <class T>
    void stairs(const char* label, const std::vector<T>& y) {
        if(_open && !y.empty()) ImPlot::PlotStairs(label, y.data(), int(y.size()));
    }
    template <class T>
    void stairs(const char* label, const std::vector<T>& x, const std::vector<T>& y) {
        const int n = int(x.size() < y.size() ? x.size() : y.size());
        if(_open && n > 0) ImPlot::PlotStairs(label, x.data(), y.data(), n);
    }
    template <class T>
    void shaded(const char* label, const std::vector<T>& x, const std::vector<T>& lo, const std::vector<T>& hi) {
        std::size_t m = x.size() < lo.size() ? x.size() : lo.size();
        m = m < hi.size() ? m : hi.size();
        if(_open && m > 0) ImPlot::PlotShaded(label, x.data(), lo.data(), hi.data(), int(m));
    }
    template <class T>
    void histogram(const char* label, const std::vector<T>& samples, int bins = -1) {
        if(_open && !samples.empty())
            ImPlot::PlotHistogram(label, samples.data(), int(samples.size()), bins > 0 ? bins : ImPlotBin_Sturges);
    }

private:
    bool _open{ false };
};

namespace plot {

template <class T>
void line(const char* title, const std::vector<T>& y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.empty()) ImPlot::PlotLine(title, y.data(), int(y.size()));
        ImPlot::EndPlot();
    }
}
template <class T>
void scatter(const char* title, const std::vector<T>& y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.empty()) ImPlot::PlotScatter(title, y.data(), int(y.size()));
        ImPlot::EndPlot();
    }
}
template <class T>
void bar(const char* title, const std::vector<T>& y) {
    if(ImPlot::BeginPlot(title)) {
        if(!y.empty()) ImPlot::PlotBars(title, y.data(), int(y.size()), 0.67);
        ImPlot::EndPlot();
    }
}

} // namespace plot

} // namespace smg
