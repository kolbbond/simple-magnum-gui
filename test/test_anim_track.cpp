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

    // duplicate-time keys keep insertion (FIFO) order: the first value added at
    // t=1 ends the incoming segment, the second begins the outgoing segment
    smg::Track<float> d;
    d.add(0.0f, 0.0f);
    d.add(1.0f, 100.0f); // first at t=1 -> terminates segment [0,1]
    d.add(1.0f, 200.0f); // second at t=1 -> starts segment [1,2]
    d.add(2.0f, 300.0f);
    CHECK(smgtest::approx(d.sample(1.0f), 100.0f)); // step lands on the first t=1 key
    CHECK(smgtest::approx(d.sample(1.5f), 250.0f)); // midpoint of 200->300, not 100->300

    TEST_RETURN();
}
