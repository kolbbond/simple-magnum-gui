
// test plotting tris

#include "GuiBase.hh"
#include "gui_test_util.hh"

using namespace Magnum;
using namespace Magnum::Math::Literals;

// example data to pass into callback
struct data_ex {

    std::string name = "example";

    Magnum::Shaders::VertexColorGL2D shader;

    GL::Mesh mesh;
};

int callback_fun(data_ex& mydata) {
    // example callback fun

    if(ImGui::Begin("hey mom")) {

        ImGui::Text("Hello, world!");
        if(ImGui::Button("Test Window")) {}
        if(ImGui::Button("Another Window")) {}
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0 / Double(ImGui::GetIO().Framerate),

            Double(ImGui::GetIO().Framerate));

        // draw triangle
        mydata.shader.draw(mydata.mesh);

    } // imgui end
    ImGui::End();

    // 0 means success
    return 0;
}

using namespace smg;
int main(int argc, char** argv) {

    // make our application class
    printf("make guibase application\n");
    GuiBase gui({ argc, argv });
    smgtest::frame_limit(gui);
    gui.setWindowSize(Vector2i{ 1920, 1080 });
    gui.setWindowTitle("test_loadimage");

    // setup triangle example
    struct TriangleVertex {
        Vector2 position;
        Color3 color;
    };
    const TriangleVertex vertices[]{
        { { -0.5f, -0.5f }, 0xff0000_rgbf }, /* Left vertex, red color */
        { { 0.5f, -0.5f }, 0x00ff00_rgbf }, /* Right vertex, green color */
        { { 0.0f, 0.5f }, 0x0000ff_rgbf } /* Top vertex, blue color */
    };

    data_ex mydata;

    // create triangle mesh before game loop
    mydata.mesh.setCount(Containers::arraySize(vertices))
        .addVertexBuffer(GL::Buffer{ vertices }, 0, Shaders::VertexColorGL2D::Position{}, Shaders::VertexColorGL2D::Color3{});

    gui.add_callback([&mydata]() { return callback_fun(mydata); });

    bool done = false;
    while(!done) done = !gui.mainLoopIteration();

    // exit
    gui.exit();
}
