// Headless smoke test of the installed package: umbrella header parses with only
// the exported defines, symbols link, and ScenePanel's layout matches libsmg's.
#include <smg/smg.hh>

#include <cstdio>

int main() {
    int failures = 0;

    if(smg::scene_panel_abi_size() != sizeof(smg::ScenePanel)) {
        std::printf("ScenePanel ABI mismatch: lib %zu, consumer %zu\n", smg::scene_panel_abi_size(), sizeof(smg::ScenePanel));
        ++failures;
    }

    smg::Bounds box;
    box.expand(Magnum::Vector3{ -1.0f });
    box.expand(Magnum::Vector3{ 1.0f });
    smg::Camera cam;
    cam.fit(box);
    if(!(cam.distance() > 1.0f)) {
        std::printf("Camera::fit did not move the camera back\n");
        ++failures;
    }

    std::printf("smg %s consumer: %s\n", SMG_VERSION_STRING, failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
