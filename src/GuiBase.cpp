#include "GuiBase.hh"

namespace guild {

GuiBase::GuiBase(const Arguments& arguments)
	: Platform::Application{arguments,
		  Configuration{}.setTitle("GuiBase").setWindowFlags(
			  Configuration::WindowFlag::Resizable)} {

	// create a log?
	_lg = Logger::create();

	// display display stats
	// check SDL
	int num_displays = SDL_GetNumVideoDisplays();
	_lg->msg("number of displays: %s%i%s\n", KRED, num_displays, KNRM);
	std::vector<SDL_Rect> displayBounds;
	for(int i = 0; i < num_displays; i++) {
		displayBounds.push_back(SDL_Rect());
		SDL_GetDisplayBounds(i, &displayBounds.back());
		_lg->msg("display %i bounds: %s(%i,%i)%s\n",
			i,
			KRED,
			displayBounds[i].x,
			displayBounds[i].y,
			KNRM);
	}

	// get the created window and override the position
	_window = Platform::Sdl2Application::window();
	SDL_SetWindowPosition(_window, 0, 0);

	// start an imgui context
	printf("Creating imgui context\n");
	Vector2i window_size = windowSize();

	// override?
	window_size[0] = 1920;
	window_size[1] = 1080;
	printf("window size: (%i,%i)\n", window_size[0], window_size[1]);

	_imgui = ImGuiIntegration::Context(
		Vector2{windowSize()} / dpiScaling(), windowSize(), framebufferSize());

	// create a context for implot
	// might need to connect to imgui but idk
	// ImPlot::SetImGuiContext(_imgui);
	printf("Creating implot context\n");
	ImPlot::CreateContext();

	// Set up proper blending to be used by ImGui. There's a great chance
	// you'll need this exact behavior for the rest of your scene. If not, set
	// this only for the drawFrame() call.
	GL::Renderer::setBlendEquation(
		GL::Renderer::BlendEquation::Add, GL::Renderer::BlendEquation::Add);
	GL::Renderer::setBlendFunction(
		GL::Renderer::BlendFunction::SourceAlpha, GL::Renderer::BlendFunction::OneMinusSourceAlpha);

#if !defined(MAGNUM_TARGET_WEBGL) && !defined(CORRADE_TARGET_ANDROID)
	/* Have some sane speed, please */
	// this is [ms] per frame?
	setMinimalLoopPeriod(16);
#endif
}

void GuiBase::drawBegin() {
	// setup the drawing state
	// clear buffer

	GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

	// start a new frame
	_imgui.newFrame();

	/* Enable text input, if needed */
	// get the input
	if(ImGui::GetIO().WantTextInput && !isTextInputActive())
		startTextInput();
	else if(!ImGui::GetIO().WantTextInput && isTextInputActive())
		stopTextInput();

	/* Update application cursor */
	_imgui.updateApplicationCursor(*this);

	/* Set appropriate states. If you only draw ImGui, it is sufficient to
 just enable blending and scissor test in the constructor. */
	GL::Renderer::enable(GL::Renderer::Feature::Blending);
	GL::Renderer::enable(GL::Renderer::Feature::ScissorTest);
	GL::Renderer::disable(GL::Renderer::Feature::FaceCulling);
	GL::Renderer::disable(GL::Renderer::Feature::DepthTest);
}

void GuiBase::drawEnd() {
	// draw, reset, swap

	// draw the frame to background buffer
	_imgui.drawFrame();

	/* Reset state. Only needed if you want to draw something else with
 different state after. */
	GL::Renderer::disable(GL::Renderer::Feature::Blending);
	GL::Renderer::disable(GL::Renderer::Feature::ScissorTest);
	GL::Renderer::enable(GL::Renderer::Feature::FaceCulling);
	GL::Renderer::enable(GL::Renderer::Feature::DepthTest);

	// swap background buffers and redraw to screen
	swapBuffers();
	redraw();
}

void GuiBase::drawEvent() {
	// main loop
	// this is called each frame

	//////////////////////////////////////////////////
	// setup the drawing
	drawBegin();

	// Do your drawing here?
	// @hey, how do we add in custom functions from outside while utilizing
	// the console api's from here ...
	// add static function calls from outside???
	std::pair<int, int> pos = get_window_position();
	ImGui::Text("window position: (%i,%i)", pos.first, pos.second);

	// call back function?
	draw_callbacks();

	//////////////////////////////////////////////////

	// draw and reset
	drawEnd();
}

void GuiBase::print_window_position() {
	// debug
	int x;
	int y;
	SDL_GetWindowPosition(_window, &x, &y);
	_lg->msg("window (x,y): %s(%i,%i)%s\n", KBLU, x, y, KNRM);
}

std::pair<int, int> GuiBase::get_window_position() {

	std::pair<int, int> pos;
	SDL_GetWindowPosition(_window, &pos.first, &pos.second);
	return pos;
}

void GuiBase::add_callback(ShDrawCallbackPr callback) {
	// add callback to list

	// append
	_callback_list.push_back(callback);
}

void GuiBase::draw_callbacks() {
	// draw callbacks from list

	// check if list empty
	if(_callback_list.empty()) {
	} else {

		// walk callbacks
		int num_callbacks = _callback_list.size();
		for(int i = 0; i < num_callbacks; i++) {

			// get data pointer
			ShDrawCallbackPr mycallback = _callback_list[i];

			// call the callback
			int flag = mycallback->draw();
			if(flag) printf("callback error!\n");
		}
	}
}

// setters
void GuiBase::set_window_position(int x, int y) {
	//
	SDL_SetWindowPosition(_window, x, y);
}

void GuiBase::set_window_size(int w, int h) {
	//
	SDL_SetWindowSize(_window, w, h);
}

// event handling for imgui
void GuiBase::keyPressEvent(KeyEvent& event) {
	if(_imgui.handleKeyPressEvent(event)) return;
}

void GuiBase::keyReleaseEvent(KeyEvent& event) {
	if(_imgui.handleKeyReleaseEvent(event)) return;
}

void GuiBase::mousePressEvent(MouseEvent& event) {
	if(_imgui.handleMousePressEvent(event)) return;
}

void GuiBase::mouseReleaseEvent(MouseEvent& event) {
	if(_imgui.handleMouseReleaseEvent(event)) return;
}

void GuiBase::mouseMoveEvent(MouseMoveEvent& event) {
	// let imgui handle its own events
	if(_imgui.handleMouseMoveEvent(event)) return;

	// send event to callbacks
	//	print_window_position();
	Vector2 mpos = Vector2{event.relativePosition()};
	Vector2i me = event.relativePosition();
	Vector2 delta = 3.0f * Vector2{event.relativePosition()} / Vector2{windowSize()};

	printf("mouse(%i,%i): \n", me[0], me[1]);
	printf("delta(%0.3f,%0.3f): \n", delta[0], delta[1]);
	//	ImGui::Text("check\n");
	//	ImGui::Text("mouse(%i,%i): \n", me[0], me[1]);
	//	ImGui::Text("delta(%0.3f,%0.3f): \n", delta[0], delta[1]);
	//	std::cout << me << std::endl;

	// check if list empty
	if(_callback_list.empty()) {
	} else {

		// walk callbacks
		int num_callbacks = _callback_list.size();
		for(int i = 0; i < num_callbacks; i++) {

			// get data pointer
			ShDrawCallbackPr mycallback = _callback_list[i];

			// call the callback
			mycallback->mouseMoveEvent(event);
			//if(flag) printf("callback error!\n");
		}
	}
}

void GuiBase::mouseScrollEvent(MouseScrollEvent& event) {
	if(_imgui.handleMouseScrollEvent(event)) {
		/* Prevent scrolling the page */
		event.setAccepted();
		return;
	}
}

void GuiBase::textInputEvent(TextInputEvent& event) {
	if(_imgui.handleTextInputEvent(event)) return;
}

//////////////////////////////////////////////////
// demos

// imgui demo for reference
void GuiBase::demo_imgui() {
	// reference

	/* 1. Show a simple window.
 Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appear in
 a window called "Debug" automatically */
	{
		ImGui::Text("Hello, world!");
		ImGui::SliderFloat("Float", &_floatValue, 0.0f, 1.0f);
		if(ImGui::ColorEdit3("Clear Color", _clearColor.data()))
			GL::Renderer::setClearColor(_clearColor);
		if(ImGui::Button("Test Window")) _showDemoWindow ^= true;
		if(ImGui::Button("Another Window")) _showAnotherWindow ^= true;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
			1000.0 / Double(ImGui::GetIO().Framerate),
			Double(ImGui::GetIO().Framerate));
	}

	/* 2. Show another simple window, now using an explicit Begin/End pair */
	if(_showAnotherWindow) {
		ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
		ImGui::Begin("Another Window", &_showAnotherWindow);
		ImGui::Text("Hello");
		ImGui::End();
	}

	/* 3. Show the ImGui demo window. Most of the sample code is in
 ImGui::ShowDemoWindow() */
	if(_showDemoWindow) {
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
		ImGui::ShowDemoWindow();
	}
}

void GuiBase::demo_implot() {
	// reference
	/* 4. Show ImPlot Demo */
	ImGui::Begin("ImPlot Demo");
	ImPlot::ShowDemoWindow();
	ImGui::End();
}

void GuiBase::demo_test() {
	// test for me???
}

void GuiBase::viewportEvent(ViewportEvent& event) {
	GL::defaultFramebuffer.setViewport({{}, event.framebufferSize()});

	_imgui.relayout(Vector2{event.windowSize()} / event.dpiScaling(),
		event.windowSize(),
		event.framebufferSize());
}
} // namespace guild
