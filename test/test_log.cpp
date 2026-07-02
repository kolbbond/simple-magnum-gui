#include "log.hh"
#include "test_util.hh"

int main() {
    smg::ShLogPr lg = smg::Log::create();
    CHECK(lg->get_num_indent() == 0);

    lg->msg(2); // indent += 2
    CHECK(lg->get_num_indent() == 2);

    lg->msg(-1);
    CHECK(lg->get_num_indent() == 1);

    // over-dedent must clamp, not go negative or abort (regression: the guard used
    // to cast the comparison result, and a stray assert aborted debug builds)
    lg->msg(-5);
    CHECK(lg->get_num_indent() == 1); // unchanged, stayed non-negative

    lg->set_num_indent(4);
    CHECK(lg->get_num_indent() == 4);

    // NullLog reports zero indentation and swallows everything
    smg::ShLogPr null = smg::NullLog::create();
    null->msg(3);
    CHECK(null->get_num_indent() == 0);

    TEST_RETURN();
}
