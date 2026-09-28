// Stands in for a consumer that never picked up smg's PUBLIC defines: the header
// is parsed here without SMG_WITH_BLOOM, so the layout must still agree with the
// one libsmg was compiled with, or ScenePanel is UB across that boundary.
#undef SMG_WITH_BLOOM

#include "ScenePanel.hh"
#include "test_util.hh"

#ifdef SMG_WITH_BLOOM
#    error "undef above did not take -- this TU would not test anything"
#endif

int main() {
    CHECK(smg::scene_panel_abi_size() == sizeof(smg::ScenePanel));
    CHECK(!smg::ScenePanel::bloom_available()); // false here regardless of libsmg
    TEST_RETURN();
}
