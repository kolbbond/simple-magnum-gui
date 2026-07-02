// telestration annotations: animatable 2D marks in normalized [0,1] image-space
#pragma once

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include <Magnum/GL/Texture.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>

#include "Anim.hh"
#include "Easing.hh"

namespace smg {

enum class AnnotationKind { Stroke, Line, Arrow, Circle, Text };

struct Annotation {
    AnnotationKind kind{ AnnotationKind::Stroke };
    std::vector<Magnum::Vector2> points; // normalized [0,1]
    Magnum::Color4 color{ 1.0f };
    float thickness{ 2.0f }; // screen px
    std::string text;
    float appear_time{ 0.0f };
    float draw_on{ 0.0f };
    Easing ease{ Easing::CubicOut };
    bool fade_in{ true };
};

// eased reveal progress [0,1] at timeline time t
[[nodiscard]] inline float annotation_progress(const Annotation& a, float t) {
    if(t < a.appear_time) return 0.0f;
    if(a.draw_on <= 0.0f) return 1.0f;
    float p = (t - a.appear_time) / a.draw_on;
    if(p < 0.0f) p = 0.0f;
    if(p > 1.0f) p = 1.0f;
    return apply(a.ease, p);
}

// normalized point -> screen pixels over the image rect
[[nodiscard]] inline Magnum::Vector2 to_screen(const Magnum::Vector2& norm,
    const Magnum::Vector2& image_min,
    const Magnum::Vector2& image_size) {
    return image_min + norm * image_size;
}

class AnnotationLayer {
public:
    std::size_t add(const Annotation& a) {
        _annotations.push_back(a);
        return _annotations.size() - 1;
    }
    // handles are indices; add()/clear() invalidate previously returned references
    [[nodiscard]] Annotation& at(std::size_t handle) {
        assert(handle < _annotations.size());
        return _annotations[handle];
    }
    [[nodiscard]] const Annotation& at(std::size_t handle) const {
        assert(handle < _annotations.size());
        return _annotations[handle];
    }
    [[nodiscard]] std::size_t size() const { return _annotations.size(); }
    void clear() { _annotations.clear(); }
    [[nodiscard]] Timeline& timeline() { return _timeline; }

    // render background + overlay at the timeline's current time (caller advances the timeline)
    void draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size);

private:
    std::vector<Annotation> _annotations;
    Timeline _timeline;
};

} // namespace smg
