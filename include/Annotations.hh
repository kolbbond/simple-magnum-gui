// telestration annotations: animatable 2D marks in normalized [0,1] image-space
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Vector2.h>

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

} // namespace smg
