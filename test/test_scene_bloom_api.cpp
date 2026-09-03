// headless: the bloom settings API is compiled on every platform, so it is
// exercised without a GL context (ScenePanel defers all GL to ensure_gl())
#include "ScenePanel.hh"
#include "test_util.hh"

int main() {
    smg::ScenePanel panel;

    // defaults match the slider start values in draw()
    CHECK(panel.bloom_enabled());
    CHECK(smgtest::approx(panel.bloom_strength(), 0.3f));
    CHECK(smgtest::approx(panel.bloom_radius(), 0.005f));

    panel.set_bloom_enabled(false);
    CHECK(!panel.bloom_enabled());
    panel.set_bloom_enabled(true);
    CHECK(panel.bloom_enabled());

    panel.set_bloom_strength(0.75f);
    CHECK(smgtest::approx(panel.bloom_strength(), 0.75f));
    panel.set_bloom_radius(0.01f);
    CHECK(smgtest::approx(panel.bloom_radius(), 0.01f));

    // out-of-range values clamp to the slider bounds rather than aborting
    panel.set_bloom_strength(-1.0f);
    CHECK(smgtest::approx(panel.bloom_strength(), 0.0f));
    panel.set_bloom_strength(5.0f);
    CHECK(smgtest::approx(panel.bloom_strength(), 1.0f));
    panel.set_bloom_radius(0.0f);
    CHECK(smgtest::approx(panel.bloom_radius(), 0.001f));
    panel.set_bloom_radius(1.0f);
    CHECK(smgtest::approx(panel.bloom_radius(), 0.02f));

    // the availability flag must agree with how the library was built
#ifdef SMG_WITH_BLOOM
    CHECK(smg::ScenePanel::bloom_available());
#else
    CHECK(!smg::ScenePanel::bloom_available());
#endif

    TEST_RETURN();
}
