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

#include "GuiBase.hh"
#include "DrawCallback.hh"
#include "imgui.h"

#include <iostream>

// example data to pass into callback
struct data_ex {
	int x;
	int y;

	std::string name = "example";

	GL::Texture2D texture;
	Containers::Optional<Trade::ImageData2D> image;
};

using namespace smg;

int callback_fun(void* data) {
	// example callback fun

	// cast our data to be meaningful
	data_ex* mydata = reinterpret_cast<data_ex*>(data);

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

		if(ImGui::BeginChild("child")) {

			//printf("texture id: ");
			//Debug{}<<mydata->texture.id();
			//mydata->texture.setImage(0, GL::TextureFormat::RGBA8, *mydata->image);

			// draw texture as image in sub ImGui window
			Magnum::ImGuiIntegration::image(
				mydata->texture, Vector2{400.0, 300.0}, Range2D{{}, Vector2{1.0f}}, Color4{1.0f}, Color4{1.0f});

			ImGui::SameLine();
			Magnum::ImGuiIntegration::image(
				mydata->texture, Vector2{400.0, 300.0}, Range2D{{}, Vector2{1.0f}}, Color4{1.0f}, Color4{0.5f});

			ImGui::Image(static_cast<ImTextureID>(&mydata->texture), {400, 300}, ImVec2(0, 0), ImVec2(0, 0));
			ImGui::SameLine();

			ImGui::Image(static_cast<ImTextureID>(&mydata->texture), {400, 300}, ImVec2(0, 0), ImVec2(1, 1));
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
	mydata.x = 5;
	mydata.y = 6;
	mydata.name = "heymom";

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
