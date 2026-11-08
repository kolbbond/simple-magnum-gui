// unit test: LogPane storage (no GL context)
#include "LogPane.hh"
#include "test_util.hh"

#include <stdexcept>
#include <string>

using namespace smg;

static void add_splits_lines() {
    LogPane p;
    p.add(LogPane::Level::Info, "a\nbb\nccc");
    CHECK(p.line_count() == 3);
    CHECK(p.line(0) == "a");
    CHECK(p.line(1) == "bb");
    CHECK(p.line(2) == "ccc");
}

static void trailing_newline_no_empty_line() {
    LogPane p;
    p.add(LogPane::Level::Warn, "x\n");
    CHECK(p.line_count() == 1);
    p.add(LogPane::Level::Warn, "");
    CHECK(p.line_count() == 1); // empty input adds nothing
    p.add(LogPane::Level::Warn, "a\n\nb");
    CHECK(p.line_count() == 4); // interior blank line kept
    CHECK(p.line(2).empty());
}

static void cap_drops_oldest() {
    LogPane p(1024);
    for(int i = 0; i < 128; ++i) p.add(LogPane::Level::Info, "line" + std::to_string(i) + std::string(28, 'x')); // ~4 KiB
    size_t total = 0;
    for(size_t i = 0; i < p.line_count(); ++i) total += p.line(i).size();
    CHECK(total <= 1024);
    CHECK(p.line_count() > 0 && p.line_count() < 128);
    CHECK(p.line(p.line_count() - 1).substr(0, 7) == "line127");
    CHECK(p.line(0).substr(0, 5) != "line0");
}

static void oversized_line_truncated() {
    LogPane p(16);
    p.add(LogPane::Level::Info, std::string(100, 'z'));
    CHECK(p.line_count() == 1);
    CHECK(p.line(0).size() == 16);
}

static void as_log_forwards() {
    LogPane p;
    ShLogPr log = p.as_log();
    log->msg("hello %d\n", 42);
    CHECK(p.line_count() == 1);
    CHECK(p.line(0) == "hello 42");
}

static void out_of_range_and_clear() {
    LogPane p;
    bool thrown = false;
    try {
        p.line(0);
    } catch(const std::out_of_range&) { thrown = true; }
    CHECK(thrown);
    p.add(LogPane::Level::Error, "e");
    p.clear();
    CHECK(p.line_count() == 0);
}

int main() {
    add_splits_lines();
    trailing_newline_no_empty_line();
    cap_drops_oldest();
    oversized_line_truncated();
    as_log_forwards();
    out_of_range_and_clear();
    TEST_RETURN();
}
