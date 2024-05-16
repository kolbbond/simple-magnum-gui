#include "GuiBase.hh"
#include "cstdio"

using namespace guild;

int main(int argc, char** argv) {

	// make the gui class (inherits Platform)
	// not a pointer as we don't need to pass it around?
	GuiBase gui({argc, argv});

	// exec calls mainloopiteration a bunch
	// this checks events and draws
	bool done = false;
	while(!done) {
		done = gui.mainLoopIteration();
		printf("%d\n", done);

		// get events?
	}

	// exit
	gui.exit();
}
