#include "Anim.hh"
#include "test_util.hh"

int main() {
    smg::Timeline tl{ 10.0f };
    CHECK(smgtest::approx(tl.time(), 0.0f));
    CHECK(!tl.playing());

    // paused: advance does nothing
    tl.advance(1.0f);
    CHECK(smgtest::approx(tl.time(), 0.0f));

    tl.play();
    tl.advance(3.0f);
    CHECK(smgtest::approx(tl.time(), 3.0f));
    CHECK(tl.playing());

    // speed scales advance
    tl.set_speed(2.0f);
    tl.advance(1.0f);
    CHECK(smgtest::approx(tl.time(), 5.0f));

    // non-loop clamps at duration and finishes
    tl.set_speed(1.0f);
    tl.advance(100.0f);
    CHECK(smgtest::approx(tl.time(), 10.0f));
    CHECK(tl.finished());
    CHECK(!tl.playing());

    // seek clamps
    tl.seek(-5.0f);
    CHECK(smgtest::approx(tl.time(), 0.0f));
    tl.seek(999.0f);
    CHECK(smgtest::approx(tl.time(), 10.0f));

    // loop wraps
    smg::Timeline lp{ 4.0f };
    lp.set_loop(true);
    lp.play();
    lp.advance(5.0f); // wraps to 1.0
    CHECK(smgtest::approx(lp.time(), 1.0f));
    CHECK(!lp.finished()); // loop never finishes

    // stop resets
    lp.stop();
    CHECK(smgtest::approx(lp.time(), 0.0f));
    CHECK(!lp.playing());

    // reverse playback terminates at the start (symmetric with the end)
    smg::Timeline rev{ 10.0f };
    rev.seek(5.0f);
    rev.set_speed(-1.0f);
    rev.play();
    rev.advance(100.0f);
    CHECK(smgtest::approx(rev.time(), 0.0f));
    CHECK(!rev.playing()); // regression: reverse used to keep playing forever

    // a zero-duration non-looping timeline is finished (nothing to play)
    smg::Timeline zero{ 0.0f };
    CHECK(zero.finished());

    TEST_RETURN();
}
