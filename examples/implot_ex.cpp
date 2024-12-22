// implot testing
#include "GuiBase.hh"

#include <iostream>

int main(int argc, char** argv) {

	// make our application class
    smg::GuiBase gui({argc, argv});

	// exec calls mainloopiteration a bunch
	// this checks events and draws
	bool done = false;
	while(!done) {
		done = !gui.mainLoopIteration();
	}

	// exit
	gui.exit();
}
