// implot testing
#include "GuiBase.hh"

#include <iostream>

int main(int argc, char** argv) {

	// make our application class
	GuiBase gui({argc, argv});

	// exec calls mainloopiteration a bunch
	// this checks events and draws
    std::string input;
    std::cin >> input;
	bool done = false;
	while(!done) {
		done = !gui.mainLoopIteration();
		// printf("%d\n", done);

		// get events?
	}

	// exit
	gui.exit();
}
