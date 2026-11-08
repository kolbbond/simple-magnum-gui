// unit test: LogPane storage (no GL context)
#include "LogPane.hh"
#include "test_util.hh"

#include "imgui.h"

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
    CHECK(p.line(0).size() == 15); // cap-1: the newline is charged
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

static void blank_flood_bounded() {
    LogPane p(64);
    p.add(LogPane::Level::Info, std::string(1000, '\n'));
    CHECK(p.line_count() <= 64);
    LogPane z(0);
    for(int i = 0; i < 100; ++i) z.add(LogPane::Level::Info, "abc\n\n");
    CHECK(z.line_count() == 1);
}

static void utf8_truncation_whole() {
    LogPane p(6); // text limit 5 bytes
    p.add(LogPane::Level::Info, "ab\xC3\xA9\xC3\xA9\xC3\xA9"); // ab + 3x 2-byte char
    CHECK(p.line(0) == "ab\xC3\xA9"); // 5th byte would split a char
}

static void as_log_strips_ansi_and_levels() {
    LogPane p;
    ShLogPr log = p.as_log();
    log->msg("%sbegin%s\n", SMG_KYEL, SMG_KNRM);
    log->msg("%sbad%s\n", SMG_KRED, SMG_KNRM);
    log->msg("plain\n");
    CHECK(p.line_count() == 3);
    CHECK(p.line(0) == "begin");
    CHECK(p.line(1) == "bad");
    CHECK(p.line(2) == "plain");
}

static void as_log_buffers_partial() {
    LogPane p;
    {
        ShLogPr log = p.as_log();
        log->msg("a ");
        CHECK(p.line_count() == 0);
        log->msg("b\n");
        CHECK(p.line_count() == 1);
        CHECK(p.line(0) == "a b");
        log->newl();
        CHECK(p.line_count() == 2);
        CHECK(p.line(1).empty());
        log->msg("tail");
        CHECK(p.line_count() == 2);
    }
    CHECK(p.line_count() == 3); // flushed on destruction
    CHECK(p.line(2) == "tail");
}

static void as_log_indent() {
    LogPane p;
    ShLogPr log = p.as_log();
    log->msg(2, "head\n"); // indent applied before the call (0), then +2
    log->msg("body\n");
    log->msg(-2, "x\n");
    log->msg("end\n");
    CHECK(p.line(0) == "head");
    CHECK(p.line(1) == "  body");
    CHECK(p.line(2) == "  x");
    CHECK(p.line(3) == "end");
}

static void two_panes_one_window() {
    ImGui::CreateContext();
    ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    ImGui::GetIO().IniFilename = nullptr;
    {
        LogPane a, b;
        for(int i = 0; i < 50; ++i) {
            a.add(LogPane::Level::Tool, "a" + std::to_string(i));
            b.add(LogPane::Level::Error, "b" + std::to_string(i));
        }
        for(int f = 0; f < 4; ++f) {
            ImGuiIO& io = ImGui::GetIO();
            io.DisplaySize = { 800, 600 };
            io.DeltaTime = 1.f / 60.f;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos({ 0, 0 });
            ImGui::SetNextWindowSize({ 800, 600 });
            ImGui::Begin("w");
            a.draw();
            b.draw();
            ImGui::End();
            a.draw("own");
            ImGui::Render();
        }
        CHECK(a.line_count() == 50);
    }
    ImGui::DestroyContext();
}

int main() {
    add_splits_lines();
    trailing_newline_no_empty_line();
    cap_drops_oldest();
    oversized_line_truncated();
    as_log_forwards();
    blank_flood_bounded();
    utf8_truncation_whole();
    as_log_strips_ansi_and_levels();
    as_log_buffers_partial();
    as_log_indent();
    two_panes_one_window();
    out_of_range_and_clear();
    TEST_RETURN();
}
