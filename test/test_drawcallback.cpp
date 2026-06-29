// Headless unit tests for smg::DrawCallback.
//
// DrawCallback is pure logic (function pointers + a user-data pointer) and
// needs no GL context, so these run in CI under `ctest -L unit`.

#include <Corrade/TestSuite/Tester.h>

#include "DrawCallback.hh"

namespace {

// free callbacks matching `typedef int (*draw_callback)(void*)`
int returns_42(void*) { return 42; }
int reads_data(void* data) { return *static_cast<int*>(data); }

struct DrawCallbackTest: Corrade::TestSuite::Tester {
	explicit DrawCallbackTest();

	void defaultDrawIsSafe();
	void drawInvokesCallback();
	void dataRoundTrip();
	void callbackReceivesData();
};

DrawCallbackTest::DrawCallbackTest() {
	addTests({&DrawCallbackTest::defaultDrawIsSafe,
		&DrawCallbackTest::drawInvokesCallback,
		&DrawCallbackTest::dataRoundTrip,
		&DrawCallbackTest::callbackReceivesData});
}

// Pins the fix for the uninitialized-_callback UB: a DrawCallback created
// without a callback (the create()->set_callback path) must be safe to
// draw() and report "did nothing" rather than jumping through garbage.
void DrawCallbackTest::defaultDrawIsSafe() {
	smg::ShDrawCallbackPr cb = smg::DrawCallback::create();
	CORRADE_COMPARE(cb->draw(), 0);
}

void DrawCallbackTest::drawInvokesCallback() {
	smg::ShDrawCallbackPr cb = smg::DrawCallback::create();
	cb->set_callback(returns_42);
	CORRADE_COMPARE(cb->draw(), 42);
}

void DrawCallbackTest::dataRoundTrip() {
	int payload = 7;
	smg::ShDrawCallbackPr cb = smg::DrawCallback::create();
	cb->set_data(&payload);
	CORRADE_COMPARE(cb->get_data(), &payload);
}

void DrawCallbackTest::callbackReceivesData() {
	int payload = 123;
	smg::ShDrawCallbackPr cb = smg::DrawCallback::create();
	cb->set_callback(reads_data);
	cb->set_data(&payload);
	CORRADE_COMPARE(cb->draw(), 123);
}

} // namespace

CORRADE_TEST_MAIN(DrawCallbackTest)
