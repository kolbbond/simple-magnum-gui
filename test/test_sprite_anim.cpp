#include "SpriteTypes.hh"
#include "test_util.hh"

int main() {
    const smg::SpriteClip walk{ 0, 7, 10.0f }; // 8 frames at 10 fps

    CHECK(walk.frame_at(0.0f) == 0);
    CHECK(walk.frame_at(0.05f) == 0); // 0.5 frames -> floor 0
    CHECK(walk.frame_at(0.15f) == 1); // 1.5 frames -> 1
    CHECK(walk.frame_at(0.8f) == 0); // 8 frames elapsed -> wraps to 0
    CHECK(walk.frame_at(0.85f) == 0); // wraps within range

    // clip starting mid-sheet keeps the offset
    const smg::SpriteClip attack{ 16, 19, 4.0f };
    CHECK(attack.frame_at(0.0f) == 16);
    CHECK(attack.frame_at(0.30f) == 17); // 1.2 frames -> 17
    CHECK(attack.frame_at(1.0f) == 16); // 4 frames -> wrap

    // fps <= 0 holds the first frame
    const smg::SpriteClip still{ 5, 9, 0.0f };
    CHECK(still.frame_at(123.0f) == 5);

    // 8-direction row selection, wrapping
    CHECK(smg::dir_row(0.0f, 8) == 0);
    CHECK(smg::dir_row(45.0f, 8) == 1);
    CHECK(smg::dir_row(360.0f, 8) == 0);
    CHECK(smg::dir_row(-45.0f, 8) == 7);

    TEST_RETURN();
}
