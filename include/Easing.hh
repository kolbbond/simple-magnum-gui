// easing curves: pure t in [0,1] -> eased [0,1], endpoints exact
#pragma once

namespace smg {

enum class Easing { Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut };

namespace ease {

[[nodiscard]] inline float linear(float t) { return t; }
[[nodiscard]] inline float quad_in(float t) { return t * t; }
[[nodiscard]] inline float quad_out(float t) { return t * (2.0f - t); }
[[nodiscard]] inline float quad_in_out(float t) { return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f; }
[[nodiscard]] inline float cubic_in(float t) { return t * t * t; }
[[nodiscard]] inline float cubic_out(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}
[[nodiscard]] inline float cubic_in_out(float t) {
    if(t < 0.5f) return 4.0f * t * t * t;
    const float u = -2.0f * t + 2.0f;
    return 1.0f - u * u * u * 0.5f;
}

} // namespace ease

[[nodiscard]] inline float apply(Easing e, float t) {
    switch(e) {
    case Easing::QuadIn: return ease::quad_in(t);
    case Easing::QuadOut: return ease::quad_out(t);
    case Easing::QuadInOut: return ease::quad_in_out(t);
    case Easing::CubicIn: return ease::cubic_in(t);
    case Easing::CubicOut: return ease::cubic_out(t);
    case Easing::CubicInOut: return ease::cubic_in_out(t);
    case Easing::Linear: break;
    }
    return ease::linear(t);
}

} // namespace smg
