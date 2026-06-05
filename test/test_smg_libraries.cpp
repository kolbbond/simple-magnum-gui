// Test that all libraries bundled with smg are properly linked and accessible
// This test verifies: Magnum, Corrade, ImGui, ImPlot, and optionally ImPlot3D

#include "GuiBase.hh"
#include "DrawCallback.hh"

// Magnum includes - verify these are accessible through smg
#include <Magnum/Magnum.h>
#include <Magnum/GL/Buffer.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Shaders/FlatGL.h>
#include <Magnum/Shaders/PhongGL.h>
#include <Magnum/Shaders/VertexColorGL.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/UVSphere.h>
#include <Magnum/Trade/MeshData.h>

// Corrade includes
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Utility/Debug.h>

// ImGui - should be available through smg
#include "imgui.h"

// ImPlot - should be available through smg
#include "implot.h"

// ImPlot3D - available when built with ENABLE_IMPLOT3D
// Check if the header exists (it will if smg was built with ImPlot3D support)
#if __has_include("implot3d.h")
#define HAS_IMPLOT3D 1
#include "implot3d.h"
#else
#define HAS_IMPLOT3D 0
#endif

#include <cstdio>

using namespace Magnum;
using namespace Magnum::Math::Literals;
using namespace smg;

// Track test results
struct TestResults {
    bool magnum_math = false;
    bool magnum_gl = false;
    bool magnum_shaders = false;
    bool magnum_primitives = false;
    bool magnum_meshtools = false;
    bool corrade_containers = false;
    bool imgui = false;
    bool implot = false;
    bool implot3d = false;
    bool smg_guibase = false;
    bool smg_callback = false;
};

static TestResults results;

int test_callback(void* data) {
    TestResults* r = reinterpret_cast<TestResults*>(data);

    // Test ImGui functionality
    if (ImGui::Begin("Library Test Results")) {
        ImGui::Text("SMG Library Linkage Test");
        ImGui::Separator();

        // Display test results
        ImGui::Text("Magnum Math:      %s", r->magnum_math ? "PASS" : "FAIL");
        ImGui::Text("Magnum GL:        %s", r->magnum_gl ? "PASS" : "FAIL");
        ImGui::Text("Magnum Shaders:   %s", r->magnum_shaders ? "PASS" : "FAIL");
        ImGui::Text("Magnum Primitives:%s", r->magnum_primitives ? "PASS" : "FAIL");
        ImGui::Text("Magnum MeshTools: %s", r->magnum_meshtools ? "PASS" : "FAIL");
        ImGui::Text("Corrade Containers:%s", r->corrade_containers ? "PASS" : "FAIL");
        ImGui::Text("ImGui:            %s", r->imgui ? "PASS" : "FAIL");
        ImGui::Text("ImPlot:           %s", r->implot ? "PASS" : "FAIL");
#if HAS_IMPLOT3D
        ImGui::Text("ImPlot3D:         %s", r->implot3d ? "PASS" : "FAIL");
#else
        ImGui::Text("ImPlot3D:         SKIPPED (not enabled)");
#endif
        ImGui::Text("SMG GuiBase:      %s", r->smg_guibase ? "PASS" : "FAIL");
        ImGui::Text("SMG Callback:     %s", r->smg_callback ? "PASS" : "FAIL");

        ImGui::Separator();

        // Test ImPlot within this callback
        // Set a minimum size to ensure the plot can be drawn
        ImGui::SetNextItemWidth(200);
        if (ImPlot::BeginPlot("Test Plot", ImVec2(300, 200))) {
            static float xs[] = {0, 1, 2, 3, 4};
            static float ys[] = {0, 1, 4, 9, 16};
            ImPlot::PlotLine("y=x^2", xs, ys, 5);
            ImPlot::EndPlot();
        }
        // If we got here without crashing, ImPlot context is working
        // Check that ImPlot context exists
        if (ImPlot::GetCurrentContext() != nullptr) {
            r->implot = true;
        }

#if HAS_IMPLOT3D
        // Test ImPlot3D if available
        // Note: smg doesn't create ImPlot3D context by default, so we create one here
        static bool implot3d_initialized = false;
        static ImPlot3DContext* implot3d_ctx = nullptr;
        if (!implot3d_initialized) {
            implot3d_ctx = ImPlot3D::CreateContext();
            implot3d_initialized = true;
        }
        if (implot3d_ctx != nullptr) {
            if (ImPlot3D::BeginPlot("Test 3D Plot", ImVec2(300, 200))) {
                ImPlot3D::EndPlot();
            }
            r->implot3d = true;
        }
#endif
    }
    ImGui::End();

    // If we got here, ImGui works
    r->imgui = true;
    r->smg_callback = true;

    return 0;
}

int main(int argc, char** argv) {
    printf("=== SMG Library Linkage Test ===\n\n");

    // Test 1: Corrade Containers
    printf("Testing Corrade Containers... ");
    {
        Corrade::Containers::Array<int> arr{5};
        arr[0] = 42;
        Corrade::Containers::ArrayView<int> view = arr;
        Corrade::Containers::Optional<int> opt{123};
        if (arr[0] == 42 && view.size() == 5 && opt && *opt == 123) {
            results.corrade_containers = true;
            printf("PASS\n");
        } else {
            printf("FAIL\n");
        }
    }

    // Test 2: Magnum Math
    printf("Testing Magnum Math... ");
    {
        Vector3 v1{1.0f, 2.0f, 3.0f};
        Vector3 v2{4.0f, 5.0f, 6.0f};
        Vector3 v3 = v1 + v2;
        Matrix4 m = Matrix4::translation(v1);
        Matrix4 r = Matrix4::rotationY(45.0_degf);
        Matrix4 combined = m * r;  // Test matrix multiplication
        Color4 c = 0xff0000ff_rgbaf;
        // Check basic vector math and matrix translation column
        (void)combined;  // Silence unused warning
        if (v3.x() == 5.0f && m.translation() == v1 && c.r() == 1.0f) {
            results.magnum_math = true;
            printf("PASS\n");
        } else {
            printf("FAIL\n");
        }
    }

    // Test 3: Create GuiBase (tests Magnum GL context creation)
    printf("Testing SMG GuiBase creation... ");
    GuiBase gui({argc, argv});
    results.smg_guibase = true;
    printf("PASS\n");

    // Test 4: Magnum GL (requires GL context from GuiBase)
    printf("Testing Magnum GL... ");
    {
        GL::Buffer buffer;
        GL::Mesh mesh;
        // If we can create these without crashing, GL is working
        results.magnum_gl = true;
        printf("PASS\n");
    }

    // Test 5: Magnum Shaders
    printf("Testing Magnum Shaders... ");
    {
        Shaders::FlatGL2D flat2d;
        Shaders::FlatGL3D flat3d;
        Shaders::VertexColorGL2D vertexColor2d;
        Shaders::VertexColorGL3D vertexColor3d;
        Shaders::PhongGL phong;
        results.magnum_shaders = true;
        printf("PASS\n");
    }

    // Test 6: Magnum Primitives
    printf("Testing Magnum Primitives... ");
    {
        Trade::MeshData cube = Primitives::cubeSolid();
        Trade::MeshData sphere = Primitives::uvSphereSolid(8, 16);
        if (cube.vertexCount() > 0 && sphere.vertexCount() > 0) {
            results.magnum_primitives = true;
            printf("PASS\n");
        } else {
            printf("FAIL\n");
        }
    }

    // Test 7: Magnum MeshTools
    printf("Testing Magnum MeshTools... ");
    {
        Trade::MeshData cube = Primitives::cubeSolid();
        GL::Mesh compiledMesh = MeshTools::compile(cube);
        if (compiledMesh.count() > 0) {
            results.magnum_meshtools = true;
            printf("PASS\n");
        } else {
            printf("FAIL\n");
        }
    }

    // Setup callback to test ImGui/ImPlot in the render loop
    printf("Testing SMG DrawCallback... ");
    ShDrawCallbackPr callback = DrawCallback::create();
    callback->set_callback(test_callback);
    callback->set_data(&results);
    gui.add_callback(callback);
    printf("PASS\n");

    // Run one frame to test ImGui and ImPlot
    printf("Testing ImGui and ImPlot (running one frame)... ");
    gui.mainLoopIteration();
    printf("DONE\n");

    // Print summary
    printf("\n=== Test Summary ===\n");
    int passed = 0;
    int total = 11;

    auto check = [&passed](bool result, const char* name) {
        printf("  %-20s: %s\n", name, result ? "PASS" : "FAIL");
        if (result) passed++;
    };

    check(results.corrade_containers, "Corrade Containers");
    check(results.magnum_math, "Magnum Math");
    check(results.magnum_gl, "Magnum GL");
    check(results.magnum_shaders, "Magnum Shaders");
    check(results.magnum_primitives, "Magnum Primitives");
    check(results.magnum_meshtools, "Magnum MeshTools");
    check(results.smg_guibase, "SMG GuiBase");
    check(results.smg_callback, "SMG Callback");
    check(results.imgui, "ImGui");
    check(results.implot, "ImPlot");

#if HAS_IMPLOT3D
    check(results.implot3d, "ImPlot3D");
#else
    printf("  %-20s: SKIPPED\n", "ImPlot3D");
    total--;
#endif

    printf("\nTotal: %d/%d tests passed\n", passed, total);

    gui.exit();

    return (passed == total) ? 0 : 1;
}
