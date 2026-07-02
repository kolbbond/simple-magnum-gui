#include "Anim.hh"
#include "test_util.hh"

int main() {
    // empty track -> default value, zero duration
    smg::Track<float> empty;
    CHECK(empty.empty());
    CHECK(smgtest::approx(empty.duration(), 0.0f));
    CHECK(smgtest::approx(empty.sample(1.0f), 0.0f));

    smg::Track<float> t;
    t.add(0.0f, 0.0f);
    t.add(2.0f, 10.0f); // linear segment to t=2

    CHECK(!t.empty());
    CHECK(smgtest::approx(t.duration(), 2.0f));

    // clamp before first / after last
    CHECK(smgtest::approx(t.sample(-1.0f), 0.0f));
    CHECK(smgtest::approx(t.sample(5.0f), 10.0f));

    // linear interpolation midpoint
    CHECK(smgtest::approx(t.sample(1.0f), 5.0f));

    // out-of-order insertion stays sorted
    smg::Track<float> u;
    u.add(2.0f, 20.0f);
    u.add(0.0f, 0.0f);
    CHECK(smgtest::approx(u.sample(1.0f), 10.0f));

    // eased segment: cubic_in at u=0.5 -> 0.125 of the span
    smg::Track<float> e;
    e.add(0.0f, 0.0f);
    e.add(1.0f, 8.0f, smg::Easing::CubicIn);
    CHECK(smgtest::approx(e.sample(0.5f), 1.0f)); // 0.125 * 8

    // anim_lerp generic
    CHECK(smgtest::approx(smg::anim_lerp(2.0f, 4.0f, 0.25f), 2.5f));

    TEST_RETURN();
}
