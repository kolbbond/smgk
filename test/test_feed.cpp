#include "Feed.hh"
#include "fixture.hh"
#include "test_util.hh"

using smgk::Feed;
using smgk::Output;
using json = nlohmann::json;

static Feed feed_of(std::initializer_list<smgk::Msg> msgs) {
    Feed f;
    for(const auto& m : msgs)
        f.apply(m);
    return f;
}

static smgk::Msg display(const std::string& parent, json data, const std::string& kind) {
    return make_msg("display_data", parent, {{"data", data}, {"metadata", {{"smgk", {{"kind", kind}}}}}});
}

static void groups_outputs() {
    Feed f =
        feed_of({make_msg("execute_input", "A", {{"code", "print(1)"}, {"execution_count", 1}}),
                 make_msg("stream", "A", {{"name", "stdout"}, {"text", "1\n"}}), make_msg("status", "A", {{"execution_state", "idle"}})});
    CHECK(f.cells().size() == 1);
    const auto& c = f.cells()[0];
    CHECK(c.source == "print(1)" && c.exec_count == 1 && c.kind == "cell" && c.msg_id == "A");
    CHECK(c.outputs.size() == 1 && c.outputs[0].kind == Output::Kind::Stream && c.outputs[0].text == "1\n");
    CHECK(f.status() == "idle");
}

static void run_kind() {
    Feed f = feed_of({make_msg("execute_input", "R", {{"code", "x"}, {"execution_count", 2}}, "smgk")});
    CHECK(f.cells()[0].kind == "run");
}

static void note_is_markdown() {
    Feed f = feed_of({display("N", {{"text/markdown", "# hi"}, {"text/plain", "# hi"}}, "note")});
    CHECK(f.cells().size() == 1);
    CHECK(f.cells()[0].markdown && f.cells()[0].source == "# hi" && f.cells()[0].kind == "note");
}

static void push_output_only() {
    Feed f = feed_of({display("P", {{"image/png", "iVBO"}, {"text/plain", "<Image>"}}, "push")});
    const auto& c = f.cells()[0];
    CHECK(!c.markdown && c.source.empty() && c.kind == "push");
    CHECK(c.outputs.size() == 1 && c.outputs[0].kind == Output::Kind::Image && c.outputs[0].png_b64 == "iVBO");
}

static void stream_merge() {
    Feed f = feed_of({make_msg("execute_input", "A", {{"code", "x"}, {"execution_count", 1}}),
                      make_msg("stream", "A", {{"name", "stdout"}, {"text", "a\n"}}),
                      make_msg("stream", "A", {{"name", "stdout"}, {"text", "b\n"}}),
                      make_msg("stream", "A", {{"name", "stderr"}, {"text", "w\n"}})});
    const auto& o = f.cells()[0].outputs;
    CHECK(o.size() == 2 && o[0].text == "a\nb\n" && o[1].name == "stderr");
}

static void error_is_plain_text() {
    auto m = smgk::parse_wire(fixture_frames("wire_error.json"));
    Feed f;
    f.apply(*m);
    const auto& o = f.cells()[0].outputs;
    CHECK(o.size() == 1 && o[0].kind == Output::Kind::Error);
    CHECK(o[0].text.find('\x1b') == std::string::npos);
    CHECK(o[0].text.find("ZeroDivisionError") != std::string::npos);
}

static void loads_history() {
    Feed f;
    f.load_notebook(json::parse(read_fixture("history.ipynb")));
    CHECK(f.cells().size() == 3);
    CHECK(f.cells()[0].source == "print(1)" && f.cells()[0].outputs[0].text == "1\n");
    CHECK(f.cells()[1].markdown && f.cells()[1].source == "# hi");
    CHECK(f.cells()[2].outputs[0].kind == Output::Kind::Image && !f.cells()[2].outputs[0].png_b64.empty());
}

static void feed_dedupe() {
    Feed f;
    f.load_notebook(json::parse(read_fixture("history.ipynb")));
    f.apply(make_msg("execute_input", "A", {{"code", "print(1)"}, {"execution_count", 1}}));
    f.apply(make_msg("stream", "A", {{"name", "stdout"}, {"text", "1\n"}}));
    CHECK(f.cells().size() == 3);
    CHECK(f.cells()[0].outputs.size() == 1 && f.cells()[0].outputs[0].text == "1\n");
    f.apply(make_msg("execute_input", "B", {{"code", "y"}, {"execution_count", 2}}));
    CHECK(f.cells().size() == 4 && f.cells()[3].msg_id == "B");
}

static void reset_clears() {
    Feed f = feed_of({make_msg("execute_input", "A", {{"code", "x"}, {"execution_count", 1}})});
    f.reset();
    CHECK(f.cells().empty());
    f.apply(make_msg("execute_input", "A", {{"code", "x"}, {"execution_count", 1}}));
    CHECK(f.cells().size() == 1);
}

int main() {
    groups_outputs();
    run_kind();
    note_is_markdown();
    push_output_only();
    stream_merge();
    error_is_plain_text();
    loads_history();
    feed_dedupe();
    reset_clears();
    TEST_RETURN();
}
