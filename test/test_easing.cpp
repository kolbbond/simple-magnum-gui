#include "Easing.hh"
#include "test_util.hh"

int main() {
    // endpoints exact for every curve
    CHECK(smgtest::approx(smg::ease::linear(0.0f), 0.0f));
    CHECK(smgtest::approx(smg::ease::linear(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::quad_in(0.0f), 0.0f));
    CHECK(smgtest::approx(smg::ease::quad_in(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::quad_out(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::cubic_in(1.0f), 1.0f));
    CHECK(smgtest::approx(smg::ease::cubic_out(0.0f), 0.0f));

    // known midpoints
    CHECK(smgtest::approx(smg::ease::linear(0.5f), 0.5f));
    CHECK(smgtest::approx(smg::ease::quad_in(0.5f), 0.25f));
    CHECK(smgtest::approx(smg::ease::quad_in_out(0.5f), 0.5f));
    CHECK(smgtest::approx(smg::ease::cubic_in_out(0.5f), 0.5f));

    // apply() dispatches to the matching function
    CHECK(smgtest::approx(smg::apply(smg::Easing::Linear, 0.3f), 0.3f));
    CHECK(smgtest::approx(smg::apply(smg::Easing::QuadIn, 0.5f), 0.25f));
    CHECK(smgtest::approx(smg::apply(smg::Easing::CubicOut, 1.0f), 1.0f));

    TEST_RETURN();
}
