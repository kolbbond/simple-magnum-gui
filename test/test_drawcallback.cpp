// Headless unit tests for smg::DrawCallback (pure logic, no GL context).
// The legacy void* API is deprecated but must keep working until it is removed.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <Corrade/TestSuite/Tester.h>

#include <memory>

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
    void dataBoundLate();
    void legacyNullDisarms();
    void lambdaCaptures();
    void onDrawNullDisarms();
    void sharedCaptureOutlivesScope();
};

DrawCallbackTest::DrawCallbackTest() {
    addTests({ &DrawCallbackTest::defaultDrawIsSafe,
        &DrawCallbackTest::drawInvokesCallback,
        &DrawCallbackTest::dataRoundTrip,
        &DrawCallbackTest::callbackReceivesData,
        &DrawCallbackTest::dataBoundLate,
        &DrawCallbackTest::legacyNullDisarms,
        &DrawCallbackTest::lambdaCaptures,
        &DrawCallbackTest::onDrawNullDisarms,
        &DrawCallbackTest::sharedCaptureOutlivesScope });
}

// Pins the fix for the uninitialized-_callback UB: a DrawCallback created
// without a callback (goose's create()->set_callback path) must be safe to
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

// the common legacy order is set_callback() then set_data()
void DrawCallbackTest::dataBoundLate() {
    int payload = 5;
    smg::ShDrawCallbackPr cb = smg::DrawCallback::create(reads_data);
    cb->set_data(&payload);
    CORRADE_COMPARE(cb->draw(), 5);
}

// used to set an "armed" flag and call through a null pointer on the next event
void DrawCallbackTest::legacyNullDisarms() {
    smg::ShDrawCallbackPr cb = smg::DrawCallback::create(returns_42);
    cb->set_callback(nullptr);
    CORRADE_COMPARE(cb->draw(), 0);
}

void DrawCallbackTest::lambdaCaptures() {
    int calls = 0;
    smg::ShDrawCallbackPr cb = smg::DrawCallback::create([&calls]() {
        ++calls;
        return 0;
    });
    cb->draw();
    cb->draw();
    CORRADE_COMPARE(calls, 2);
}

void DrawCallbackTest::onDrawNullDisarms() {
    smg::ShDrawCallbackPr cb = smg::DrawCallback::create([]() { return 1; });
    cb->on_draw(nullptr);
    CORRADE_COMPARE(cb->draw(), 0);
}

// captured state lives as long as the callback: a shared_ptr capture can't dangle
void DrawCallbackTest::sharedCaptureOutlivesScope() {
    smg::ShDrawCallbackPr cb;
    {
        std::shared_ptr<int> state = std::make_shared<int>(9);
        cb = smg::DrawCallback::create([state]() { return *state; });
    }
    CORRADE_COMPARE(cb->draw(), 9);
}

} // namespace

CORRADE_TEST_MAIN(DrawCallbackTest)
