// GuiMain.hh

#ifndef GuiMain_hh
#define GuiMain_hh

//#include "ImPlotWrapper.hh"
#include "ImGuiWrapper.hh"
#include "WindowWrapper.hh"
#include "GuiConsole.hh"
//#include <guild>
#include <memory>

namespace guild {
typedef std::shared_ptr<class GuiMain> ShGuiMainPr;
class GuiMain {

	// properties
protected:
	// exit flag
	bool flag_done_ = false;

	// the window
	ShWindowWrapperPr window_;

	// the imgui
	ShImGuiWrapperPr imgui_;

	// the implot
	//ShImPlotWrapperPr implot_;

	// methods
public:
	// constructor
	GuiMain();

	// initialize basic
	int init();

	// event handler
	void handle_events();

	// frame step
	void new_frame();

	// demo window?
	int demo_window();

	int demo_console();

	// close everything
	int close();

	// setters
	void set_window(ShWindowWrapperPr);
	void set_imgui(ShImGuiWrapperPr);
	//void set_implot(ShImPlotWrapperPr);

	// getters
	bool get_done();

	// factory
	static ShGuiMainPr create();
};

} // namespace guild

#endif
