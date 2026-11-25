// Headless smoke test of the installed package: umbrella header parses with only
// the exported defines, symbols link, ScenePanel's layout matches libsmg's, and
// plugins load from the prefix this was configured against (catches a relocated SDK
// silently picking up the build tree's baked-in plugin dir).
#include <smg/smg.hh>

#include <Corrade/Containers/StringView.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Text/AbstractFont.h>
#include <Magnum/Trade/AbstractImporter.h>

#include <cstdio>
#include <filesystem>
#include <string>

template <class T>
int check_plugin(const char* name) {
    Corrade::PluginManager::Manager<T> manager;
    const Corrade::Containers::StringView dir = manager.pluginDirectory();
    const std::string real = std::filesystem::weakly_canonical(std::filesystem::path{ std::string{ dir.data(), dir.size() } }).string();
    const std::string prefix = std::string{ SMG_CONSUMER_PREFIX } + "/";
    if(real.compare(0, prefix.size(), prefix) != 0) {
        std::printf("%s: plugin dir %s is outside %s\n", name, real.c_str(), SMG_CONSUMER_PREFIX);
        return 1;
    }
    if(!(manager.load(name) & Corrade::PluginManager::LoadState::Loaded)) {
        std::printf("%s: failed to load from %s\n", name, real.c_str());
        return 1;
    }
    return 0;
}

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

    failures += check_plugin<Magnum::Text::AbstractFont>("FreeTypeFont");
    failures += check_plugin<Magnum::Trade::AbstractImporter>("StbImageImporter");

    std::printf("smg %s consumer: %s\n", SMG_VERSION_STRING, failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
