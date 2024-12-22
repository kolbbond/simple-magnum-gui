// implot testing
#include "GuiBase.hh"

#include <iostream>

class data {
    public:
	int x;
	int y;
};

data* data_g = new data();

typedef int (*callback_fn)(void*);
void callback(callback_fn mycb) {

    printf("in callback\n");
	mycb(data_g);
}

int mycallback(void* indata) {

	data* mydata = static_cast<data*>(indata);

    mydata->x=1;
    printf("x: %i\n",mydata->x);

    return 0;
}

int main(int argc, char** argv) {

	// make our application class
    printf("make guibase application\n");
	GuiBase gui({argc, argv});

	// exec calls mainloopiteration a bunch
	// this checks events and draws
	std::string input;
	std::cin >> input;
	bool done = false;
	while(!done) {
		done = !gui.mainLoopIteration();
		// printf("%d\n", done);
        callback(mycallback);

		// get events?
	}

	// exit
	gui.exit();
}
