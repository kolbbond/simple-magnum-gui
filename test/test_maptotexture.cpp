// implot testing
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/StringView.h>
#include <Magnum/Trade/Trade.h>
#include <Magnum/Trade/ImageData.h>
#include <Corrade/PluginManager/Manager.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <MagnumPlugins/AnyImageImporter/AnyImageImporter.h>

#include <Magnum/ImGuiIntegration/Widgets.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/AbstractFramebuffer.h>

#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"

#include <iostream>

// example data to pass into callback
struct data_ex {
	int x;
	int y;

	std::string name = "example";

	VectorTypeFor<2, int> size = {4096, 4096};
	Magnum::Shaders::VertexColorGL2D shader;
	Magnum::Shaders::FlatGL2D shader2{Magnum::NoCreate};
	GL::Mesh mesh;
	GL::Mesh mesh2;
	GL::Texture2D texture2;

	GL::Texture2D texture;
	Containers::Optional<Trade::ImageData2D> image;
};

using namespace smg;

int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex* mydata = reinterpret_cast<data_ex*>(data);

	static Shaders::FlatGL2D shaderf{Shaders::FlatGL2D::Configuration{}.setFlags(Shaders::FlatGL2D::Flag::Textured)};

	//printf("debug callback\n");

	if(ImGui::Begin("hey mom")) {

		ImGui::Text("Hello, world!");

		if(ImGui::Button("Test Window")) {
		}

		if(ImGui::Button("Another Window")) {
		}

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0 / Double(ImGui::GetIO().Framerate),
			Double(ImGui::GetIO().Framerate));
		ImGui::Text("the second image is ImGui::Image, idk why it is upside down\n");

		if(ImGui::BeginChild("child")) {

			//printf("texture id: ");
			//Debug{}<<mydata->texture.id();
			//mydata->texture.setImage(0, GL::TextureFormat::RGBA8, *mydata->image);

			// draw texture as image in sub ImGui window
			// using the integration but also direct to ImGui
			Magnum::ImGuiIntegration::image(
				mydata->texture, Vector2{400.0, 300.0}, Range2D{{}, Vector2{1.0f}}, Color4{1.0f}, Color4{1.0f});
			ImGui::SameLine();
			ImGui::Image(static_cast<ImTextureID>(&mydata->texture), {400, 300}, ImVec2(0, 0), ImVec2(1, 1));

			mydata->texture2.setStorage(1, GL::TextureFormat::RGBA8, mydata->size);

			static GL::Framebuffer framebuffer({{}, mydata->size});

			framebuffer.attachTexture(GL::Framebuffer::ColorAttachment{0}, mydata->texture2, 0);
			framebuffer.mapForDraw({{Shaders::VertexColorGL2D::ColorOutput, GL::Framebuffer::ColorAttachment(0)}});
			framebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
			framebuffer.bind();

			// draw to this framebuffer ...
			mydata->shader.draw(mydata->mesh);

			Magnum::ImGuiIntegration::image(
				mydata->texture2, Vector2{400.0, 300.0}, Range2D{{}, Vector2{1.0f}}, Color4{1.0f}, Color4{1.0f});
			ImGui::SameLine();
			ImGui::Image(static_cast<ImTextureID>(&mydata->texture2), {400, 300}, ImVec2(0, 0), ImVec2(1, 1));

			/* Switch back to the default framebuffer */
			GL::defaultFramebuffer.bind();

			// handle the triangle mesh texture
			mydata->shader.draw(mydata->mesh);

			// create second mesh with texture map
			shaderf.bindTexture(mydata->texture);
			shaderf.draw(mydata->mesh2);
			// plot a triangle with the mapped texture
		}
		ImGui::EndChild();
	}
	ImGui::End();

	// 0 means success
	return 0;
}

int main(int argc, char** argv) {
	// make our application class
	printf("make guibase application\n");
	GuiBase gui({argc, argv});
	gui.setWindowSize(Vector2i{1920, 1080});
	gui.setWindowTitle("test_loadimage");

	// example data
	data_ex mydata = data_ex();

	// load image
	PluginManager::Manager<Trade::AbstractImporter> manager;
	Containers::Pointer<Trade::AbstractImporter> importer = manager.loadAndInstantiate("AnyImageImporter");
	if(!importer || !importer->openFile("/home/kolbbond/programs/cpp/smg/tests/cute_goose.jpg")) assert(0);

	// set into data
	mydata.image = importer->image2D(0);

	if(!mydata.image) Fatal{} << "import image failed";

	Containers::Optional<Trade::ImageData2D> image = importer->image2D(0);

	CORRADE_INTERNAL_ASSERT(mydata.image);
	CORRADE_INTERNAL_ASSERT(image);

	printf("image loaded success?\n");

	mydata.texture.setWrapping(GL::SamplerWrapping::ClampToEdge);
	//mydata.texture.setStorage(1, PixelForma(mydata.image->format()), mydata.image->size());
	mydata.texture.setStorage(1, GL::TextureFormat::RGBA8, mydata.image->size());
	mydata.texture.setMagnificationFilter(GL::SamplerFilter::Linear);
	mydata.texture.setMinificationFilter(GL::SamplerFilter::Linear);
	mydata.texture.setSubImage(0, {}, *mydata.image);

	// setup FlatGL with texture
	//mydata->shader2{Shaders::FlatGL2D::Configuration{}.setFlags(Shaders::FlatGL2D::Flag::Textured)};

	// setup triangle example
	struct TriangleVertex {
		Vector2 position;
		Color3 color;
	};
	struct TextureVertex {
		Vector2 position;
		Vector2 textureCoordinates;
	};

	float x0 = -1.0f;
	float dx = 1.0f;
	const TriangleVertex vertices1[]{
		{{x0, -0.5f}, 0x000ff0_rgbf}, /* Left vertex, red color */
		{{x0 + dx, -0.5f}, 0x0ff000_rgbf}, /* Right vertex, green color */
		{{x0 + dx / 2, 0.5f}, 0x0000ff_rgbf} /* Top vertex, blue color */
	};

	// copy vertex
	TextureVertex vertices2[3];
	for(int i = 0; i < 3; i++) {
		vertices2[i].position = vertices1[i].position;
	}

	vertices2[0].textureCoordinates = {-1.0, 0};
	vertices2[1].textureCoordinates = {1, 0};
	vertices2[2].textureCoordinates = {1.0f, 1.0f};

	// shift
	for(int i = 0; i < 3; i++) {
		vertices2[i].position[0] += dx;
	}

	// create triangle mesh before game loop
	mydata.mesh.setCount(Containers::arraySize(vertices1))
		.addVertexBuffer(
			GL::Buffer{vertices1}, 0, Shaders::VertexColorGL2D::Position{}, Shaders::VertexColorGL2D::Color3{});

	// create second mesh with texture map
	mydata.mesh2.setCount(Containers::arraySize(vertices2))
		.addVertexBuffer(
			GL::Buffer{vertices2}, 0, Shaders::FlatGL2D::Position{}, Shaders::FlatGL2D::TextureCoordinates{});

	// make our call back
	ShDrawCallbackPr mycb = DrawCallback::create();
	mycb->set_callback(callback_fun);

	// reference the data here
	mycb->set_data((void*)&mydata);

	// set callback into our gui
	gui.add_callback(mycb);

	// add a separate callback
	//ShDrawCallbackPr mycb2 = DrawCallback::create();
	//mycb2->set_callback(RealtimePlot::callback);

	// add this
	//gui.add_callback(mycb2);

	// exec calls mainloopiteration a bunch
	// this checks events and draws
	std::string input;
	//std::cin >> input;
	bool done = false;
	while(!done) {
		//        printf("loop iteration\n");
		done = !gui.mainLoopIteration();

		// done is true for the test
		//        done=true;
	}

	// exit
	gui.exit();
}
