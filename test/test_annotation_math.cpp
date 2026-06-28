#include "Annotations.hh"
#include "test_util.hh"

int main() {
    // to_screen maps normalized -> pixels over the image rect
    const Magnum::Vector2 p =
        smg::to_screen(Magnum::Vector2{ 0.5f, 0.25f }, Magnum::Vector2{ 10.0f, 20.0f }, Magnum::Vector2{ 100.0f, 200.0f });
    CHECK(smgtest::approx(p.x(), 60.0f));
    CHECK(smgtest::approx(p.y(), 70.0f));

    smg::Annotation a;
    a.appear_time = 1.0f;
    a.draw_on = 2.0f;
    a.ease = smg::Easing::Linear;

    CHECK(smgtest::approx(smg::annotation_progress(a, 0.0f), 0.0f)); // before appear
    CHECK(smgtest::approx(smg::annotation_progress(a, 1.0f), 0.0f)); // at appear
    CHECK(smgtest::approx(smg::annotation_progress(a, 2.0f), 0.5f)); // halfway
    CHECK(smgtest::approx(smg::annotation_progress(a, 3.0f), 1.0f)); // done
    CHECK(smgtest::approx(smg::annotation_progress(a, 9.0f), 1.0f)); // after

    // instant (draw_on == 0): 0 before appear, 1 at/after
    smg::Annotation inst;
    inst.appear_time = 1.0f;
    inst.draw_on = 0.0f;
    CHECK(smgtest::approx(smg::annotation_progress(inst, 0.5f), 0.0f));
    CHECK(smgtest::approx(smg::annotation_progress(inst, 1.0f), 1.0f));

    TEST_RETURN();
}
