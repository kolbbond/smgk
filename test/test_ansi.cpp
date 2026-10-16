#include "Ansi.hh"
#include "fixture.hh"
#include "test_util.hh"

int main() {
    CHECK(smgk::strip_ansi("\x1b[0;31mValueError\x1b[0m: bad") == "ValueError: bad");
    CHECK(smgk::strip_ansi("plain") == "plain");
    CHECK(smgk::strip_ansi("") == "");
    CHECK(smgk::strip_ansi("trailing \x1b[") == "trailing ");

    auto m = smgk::parse_wire(fixture_frames("wire_error.json"));
    CHECK(m.has_value());
    if(m) {
        std::string tb;
        for(const auto& line : m->content["traceback"])
            tb += smgk::strip_ansi(line.get<std::string>()) + "\n";
        CHECK(tb.find('\x1b') == std::string::npos);
        CHECK(tb.find("ZeroDivisionError: division by zero") != std::string::npos);
    }
    TEST_RETURN();
}
