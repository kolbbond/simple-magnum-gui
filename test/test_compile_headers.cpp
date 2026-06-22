// Header-rot guard: including the umbrella must pull in the whole public API
// and compile cleanly on its own. If this translation unit builds, every
// public header is self-contained and include-ordered correctly.
//
// It constructs nothing GL-backed, so it runs headless under `ctest -L unit`.

#include <Corrade/TestSuite/Tester.h>

#include "smg.hh" // umbrella -- compiling this file IS the test

namespace {

struct CompileHeadersTest: Corrade::TestSuite::Tester {
	explicit CompileHeadersTest();

	void umbrellaExposesPublicApi();
};

CompileHeadersTest::CompileHeadersTest() {
	addTests({&CompileHeadersTest::umbrellaExposesPublicApi});
}

void CompileHeadersTest::umbrellaExposesPublicApi() {
	// Namespaced utility type is reachable through the umbrella.
	smg::fltp x = 1.0;
	CORRADE_COMPARE(x, smg::fltp(1.0));

	// version.hh ships through the umbrella and is preprocessor-usable.
	CORRADE_VERIFY(SMG_VERSION >= SMG_VERSION_CHECK(0, 1, 0));
	CORRADE_VERIFY(SMG_VERSION_STRING[0] != '\0');
}

} // namespace

CORRADE_TEST_MAIN(CompileHeadersTest)
