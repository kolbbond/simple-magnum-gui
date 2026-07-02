#include "error.hh"
#include "test_util.hh"

#include <stdexcept>
#include <string>
#include <type_traits>

int main() {
    // smg_error must be a std::runtime_error whose copy is nothrow (exception types must be)
    CHECK((std::is_base_of<std::runtime_error, smg::smg_error>::value));
    CHECK(std::is_nothrow_copy_constructible<smg::smg_error>::value);

    // smg_throw_line tags the message with the call site and composes inside if/else
    bool caught = false;
    if(true)
        try {
            smg_throw_line("boom");
        } catch(const std::exception& e) {
            caught = true;
            const std::string what = e.what();
            CHECK(what.find("boom") != std::string::npos);
            CHECK(what.find("test_error") != std::string::npos); // SMG_FILENAME
        }
    else
        CHECK(false); // proves the macro didn't swallow the else
    CHECK(caught);

    TEST_RETURN();
}
